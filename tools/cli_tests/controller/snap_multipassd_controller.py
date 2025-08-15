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
import os
import subprocess
from asyncio.subprocess import Process
from typing import AsyncIterator, Optional

from cli_tests.utilities import get_sudo_tool
from .controller_exceptions import ControllerPrerequisiteError


class SnapdMultipassdController:
    snap_name: str = "multipass"
    daemon_service_name: str = "multipass.multipassd"

    def __init__(self):
        if sys.platform != "linux":
            raise ControllerPrerequisiteError("Snap controller requies a Linux host.")
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

    async def _run(self, *args, timeout=30, to_completion=True):
        # run snap with sudo when necessary
        cmd = [*get_sudo_tool(), *args]
        proc = await asyncio.create_subprocess_exec(
            *cmd,
            stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.STDOUT,
            env=os.environ.copy(),
        )

        if to_completion:
            try:
                await asyncio.wait_for(proc.wait(), timeout=timeout)
            except asyncio.TimeoutError:
                proc.kill()
                await proc.wait()
                raise

            out = b""
            if proc.stdout:
                out = await proc.stdout.read()

            return proc.returncode, out.decode("utf-8", "replace").strip()

        return proc

    async def _get_status(self):
        exitcode, result = await self._run("snap", "services", self.daemon_service_name)

        if exitcode != 0:
            raise RuntimeError("failed")

        lines = result.splitlines()

        if len(lines) < 2:
            raise RuntimeError(f"No such service: {self.daemon_service_name}")

        properties = dict(
            zip(("service", "startup", "current", "notes"), lines[1].split(None, 3))
        )

        return properties["current"]

    async def start(self) -> None:
        await self._run("snap", "start", self.snap_name)
        self._active_enter_timestamp_monotonic = (
            await self._get_active_enter_timestamp_monotonic()
        )

    async def stop(self, graceful=True) -> None:
        await self._run("snap", "stop", self.snap_name)

    async def restart(self) -> None:
        await self._run("snap", "restart", self.snap_name)

    async def follow_output(self) -> AsyncIterator[str]:
        """Yield decoded log lines (utf-8, replace errors)."""
        proc = await self._run(
            "snap", "logs", self.snap_name, "-f", to_completion=False
        )
        try:
            while True:
                line = await proc.stdout.readline()
                if not line:
                    break
                yield line.decode("utf-8", "replace")
        finally:
            if proc.returncode is None:
                # Give it a moment to exit cleanly (EOF on pipes)
                try:
                    await asyncio.wait_for(proc.communicate(), timeout=0.5)
                except asyncio.TimeoutError:
                    proc.terminate()
                    try:
                        await asyncio.wait_for(proc.wait(), timeout=1.0)
                    except asyncio.TimeoutError:
                        proc.kill()
                        await proc.wait()

    async def is_active(self) -> bool:
        return await self._get_status() == "active"

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
        status, content = await self._run(
            "systemctl",
            "show",
            "-p",
            property_name,
            "--value",
            service_name,
        )
        if status == 0:
            return content

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
