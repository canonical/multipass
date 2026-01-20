#!/usr/bin/env python3
#
# Copyright (C) Canonical, Ltd.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 3.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
#

"""
Windows Service-based controller for multipassd

Implements the MultipassdController Protocol using the Windows Service
Control Manager (SCM).

Notes:
- Assumes the daemon is installed as a Windows service.

"""

import asyncio
import re
import sys
import subprocess
from typing import AsyncIterator, Optional

from cli_tests.utilities import (
    StdoutAsyncSubprocess,
    SilentAsyncSubprocess,
    sudo
)
from cli_tests.config import cfg
from .controller_exceptions import ControllerPrerequisiteError


class WindowsServiceMultipassdController:
    """SCM-backed controller for multipassd on Windows."""

    def __init__(self, service_name: Optional[str] = "multipass"):
        if sys.platform != "win32":
            raise ControllerPrerequisiteError(
                "WindowsServiceMultipassdController requires Windows.")

        if cfg.driver == "hyperv":
            result = subprocess.run([*sudo(
                'powershell', '-Command',
                'if ((Get-WindowsOptionalFeature -Online -FeatureName Microsoft-Hyper-V).State -eq "Enabled") { exit 0 } else { exit 1 }'
            )], capture_output=True, check=False)
            if not result.returncode == 0:
                raise ControllerPrerequisiteError(
                    "Hyper-V must be enabled to use the `hyperv` driver!"
                )

        self.service_name = service_name
        ret = subprocess.run(
            ["sc.exe", "query", self.service_name],
            stdin=subprocess.DEVNULL,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            check=False,
        )

        if ret.returncode != 0:
            raise ControllerPrerequisiteError(
                f"`Multipass daemon Windows service `{self.service_name}` not found!"
            )

        # Internal state used for autorestart detection
        self._daemon_pid: Optional[int] = None

    async def start(self) -> None:
        """Start the service (idempotent)."""
        async with SilentAsyncSubprocess(*sudo(
            "sc.exe", "start", self.service_name
        )) as start:
            await start.communicate()

        while not await self.is_active():
            await asyncio.sleep(0.5)

        # Cache current PID for auto-restart detection
        self._daemon_pid = await self._get_pid()

    async def stop(self, graceful: bool = True) -> None:
        """Stop the service. Uses a graceful stop; falls back to terminate."""

        async with SilentAsyncSubprocess(*sudo(
            "sc.exe", "stop", self.service_name
        )) as stop:
            await stop.communicate()
            if stop.returncode != 0:
                async with SilentAsyncSubprocess(*sudo(
                    "taskkill", "/FI", f"SERVICES eq {self.service_name}", "/F"
                )) as stop_force:
                    await stop_force.communicate()

        while await self.is_active():
            await asyncio.sleep(0.5)

    async def restart(self) -> None:
        await self.stop(graceful=True)
        await self.start()

    async def follow_output(self) -> AsyncIterator[str]:
        """Yield decoded log lines from Windows Event Log (Application).

        Implementation details:
        - Uses `wevtutil qe` with XPath filter for provider name containing
          "multipass". Because there is no simple built-in "follow" mode,
          we poll using the last seen Record ID.
        - Emits message text only; converts CRLF to LF.
        """
        # Import locally to avoid gating the whole file behind a platform check
        import win32evtlog

        loop = asyncio.get_running_loop()
        q: asyncio.Queue[str] = asyncio.Queue()

        pubmeta = win32evtlog.EvtOpenPublisherMetadata(
            "Multipass",          # provider name
        )

        def callback(action, _, handle):
            # action is one of the EVT_SUBSCRIBE_NOTIFY_* values.
            # We only care about new events.
            if action == win32evtlog.EvtSubscribeActionDeliver:
                try:
                    # Render the event message (can be expensive; done on callback thread)
                    msg = win32evtlog.EvtFormatMessage(
                        Metadata=pubmeta, Event=handle, Flags=win32evtlog.EvtFormatMessageEvent)
                except Exception as ex:
                    msg = f"<unformatted event>: {ex}"

                # Hand off to asyncio loop
                loop.call_soon_threadsafe(
                    q.put_nowait, msg.replace("\r\n", "\n"))

        signal_event = None  # you can pass a Windows event for shutdown if you want
        flags = win32evtlog.EvtSubscribeToFutureEvents
        channel = "Application"
        query = "*[System[Provider[@Name='Multipass']]]"

        sub = win32evtlog.EvtSubscribe(
            ChannelPath=channel,
            SignalEvent=signal_event,
            Query=query,
            Callback=callback,
            Flags=flags
        )

        try:
            while True:
                msg = await q.get()
                yield msg
        finally:
            sub.Close()

    async def is_active(self) -> bool:
        return await self._get_state() == "RUNNING"

    async def wait_exit(self) -> Optional[int]:
        # Wait until service is not RUNNING
        while await self.is_active():
            await asyncio.sleep(0.5)
        # Return last service exit code if available
        return await self.exit_code()

    def supports_self_autorestart(self) -> bool:
        return True

    async def wait_for_self_autorestart(self, timeout: int = 60) -> None:
        async def _wait():
            current = self._daemon_pid
            while current == self._daemon_pid:
                current = await self._get_pid()
                await asyncio.sleep(0.3)  # polling interval
            self._daemon_pid = current

        await asyncio.wait_for(_wait(), timeout)

    async def exit_code(self) -> Optional[int]:
        """Return SERVICE_EXIT_CODE if available, else WIN32_EXIT_CODE, else None."""
        info = await self._query_sc()
        if not info:
            return None
        # Prefer the service-provided exit code
        if info.get("SERVICE_EXIT_CODE") is not None:
            try:
                exit_code = re.search(
                    r'(\d+)', info["SERVICE_EXIT_CODE"]).group(1)
                return int(exit_code)
            except Exception:
                pass
        if info.get("WIN32_EXIT_CODE") is not None:
            try:
                exit_code = re.search(
                    r'(\d+)', info["WIN32_EXIT_CODE"]).group(1)
                return int(exit_code)
            except Exception:
                pass
        return None

    async def _get_state(self) -> Optional[str]:
        info = await self._query_sc()
        return info.get("STATE_NAME") if info else None

    async def _get_pid(self) -> Optional[int]:
        info = await self._query_sc()
        if not info:
            return None
        pid = info.get("PID") or info.get("PROCESS_ID")
        if pid:
            try:
                return int(pid)
            except Exception:
                return None
        return None

    async def _query_sc(self) -> dict:
        """Call `sc.exe query[ex]` and parse key/value pairs from output."""

        async with StdoutAsyncSubprocess(*sudo("sc.exe", "queryex", self.service_name)) as query:
            stdout, _ = await query.communicate()
            if query.returncode != 0:
                return {}

            text = stdout.decode("utf-8", "replace")
            info: dict[str, str] = {}

            # STATE line contains numeric code and name; keep the name separately
            m = re.search(
                r"^\s*STATE\s*:\s*\d+\s+([A-Z_]+)", text, re.MULTILINE)
            if m:
                info["STATE_NAME"] = m.group(1).strip()

            # Extract key:value pairs like PID, WIN32_EXIT_CODE, SERVICE_EXIT_CODE
            for key in ("PID", "PROCESS_ID", "WIN32_EXIT_CODE", "SERVICE_EXIT_CODE"):
                km = re.search(
                    rf"^\s*{re.escape(key)}\s*:\s*([^\r\n]+)", text, re.MULTILINE)
                if km:
                    info[key] = km.group(1).strip()

            return info
