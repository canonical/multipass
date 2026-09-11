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

import sys
import asyncio
import subprocess
import time
from asyncio.subprocess import Process
from typing import AsyncIterator, Optional

from cli.utilities import (
    sudo,
    StdoutAsyncSubprocess,
)
from .controller_exceptions import ControllerPrerequisiteError


class SnapMultipassdController:
    snap_name: str = "multipass"
    daemon_service_name: str = "multipass.multipassd"

    def __init__(self):
        if sys.platform != "linux":
            raise ControllerPrerequisiteError("Snap controller requires a Linux host.")

        self._logs_proc: Optional[Process] = None
        self._active_enter_timestamp_monotonic = None

        ret = subprocess.run(
            ["snap", "list", self.snap_name],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            check=False,
        )

        if ret.returncode != 0:
            raise ControllerPrerequisiteError(
                f"`{self.snap_name}` snap is not installed!"
            )

    async def _run_snap_service_command(self, command: str) -> None:
        """Run `snap <command> <snap>` and fail loudly if snapd rejects it.

        snapd refuses a new service command while a previous change on the
        same snap is still in flight (e.g. a `stop` that is waiting on a
        daemon that ignores SIGTERM). Swallowing that error makes the caller
        believe the daemon was (re)started and the failure then surfaces as
        a bogus "multipassd died with code 0" from the monitor.
        """
        async with StdoutAsyncSubprocess(
            *sudo("snap", command, self.snap_name)
        ) as proc:
            stdout, _ = await proc.communicate()
            if proc.returncode != 0:
                raise RuntimeError(
                    f"`snap {command} {self.snap_name}` exited "
                    f"{proc.returncode}: {stdout.decode('utf-8', 'replace').strip()}"
                )

    async def _wait_for_state(self, active: bool, timeout: float = 60) -> None:
        """Poll until the service is active or not; fail on timeout."""
        deadline = time.monotonic() + timeout
        while await self.is_active() != active:
            if time.monotonic() >= deadline:
                desired = "active" if active else "inactive"
                raise RuntimeError(
                    f"Service `{self.daemon_service_name}` did not become "
                    f"{desired} within {timeout}s"
                )
            await asyncio.sleep(0.5)

    async def start(self) -> None:
        await self._run_snap_service_command("start")
        await self._wait_for_state(active=True)

        self._active_enter_timestamp_monotonic = (
            await self._get_active_enter_timestamp_monotonic()
        )

    async def stop(self, graceful=True) -> None:
        await self._run_snap_service_command("stop")
        await self._wait_for_state(active=False)

    async def restart(self) -> None:
        await self._run_snap_service_command("restart")
        await self._wait_for_state(active=True)

    async def follow_output(self) -> AsyncIterator[str]:
        """Yield decoded log lines (utf-8, replace errors)."""

        async with StdoutAsyncSubprocess(
            *sudo("snap", "logs", self.snap_name, "-f")
        ) as logs_proc:
            while True:
                line = await logs_proc.stdout.readline()
                if not line:
                    break
                yield line.decode("utf-8", "replace")

    async def is_active(self) -> bool:
        # Ask systemd directly: `snap services` only reports active/inactive
        # and folds the transitional "activating"/"deactivating" states into
        # "inactive". A daemon that is still shutting down (stop-sigterm) must
        # count as active so wait_exit() keeps polling until it really exits.
        state = await self._get_systemctl_property(
            f"snap.{self.daemon_service_name}.service", "ActiveState"
        )
        return (state or "").strip() in (
            "active",
            "activating",
            "deactivating",
            "reloading",
        )

    async def wait_exit(self) -> Optional[int]:
        """Return exit code if available; else None. Should return promptly if stopped."""
        # This will wait for exit.
        while await self.is_active():
            await asyncio.sleep(0.5)
        return await self.exit_code()

    async def wait_for_self_autorestart(self, timeout=60):
        async def _wait():
            current = self._active_enter_timestamp_monotonic
            while current == self._active_enter_timestamp_monotonic:
                current = await self._get_active_enter_timestamp_monotonic()
                await asyncio.sleep(0.3)  # polling interval
            self._active_enter_timestamp_monotonic = current

        await asyncio.wait_for(_wait(), timeout)

    def supports_self_autorestart(self) -> bool:
        return True

    async def _get_systemctl_property(
        self, service_name, property_name
    ) -> Optional[str]:
        async with StdoutAsyncSubprocess(
            *sudo("systemctl", "show", "-p", property_name, "--value", service_name)
        ) as sysctl:
            stdout, _ = await sysctl.communicate()
            if sysctl.returncode == 0:
                return stdout.decode("utf-8")

    async def _get_active_enter_timestamp_monotonic(self):
        r = await self._get_systemctl_property(
            f"snap.{self.daemon_service_name}.service", "ActiveEnterTimestampMonotonic"
        )
        return int(r) if r else None

    async def exit_code(self) -> Optional[int]:
        r = await self._get_systemctl_property(
            f"snap.{self.daemon_service_name}.service", "ExecMainStatus"
        )
        return int(r) if r else None
