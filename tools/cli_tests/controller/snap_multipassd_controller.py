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
from asyncio.subprocess import Process
from typing import AsyncIterator, Optional

from cli_tests.utilities import get_sudo_tool


class SnapdMultipassdController:
    service_name: str = "multipass"
    daemon_service_name: str = "multipass.multipassd"

    def __init__(self):
        if sys.platform == "win32":
            raise RuntimeError("Snap backend requires Linux host.")
        self._logs_proc: Optional[Process] = None

        # stop if already running
        # Check if installed?
        # sudo snap list multipass
        # multipass  1.16.0   15347  latest/stable  canonical✓  -

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
        await self._run("snap", "start", self.service_name)

    async def stop(self, graceful=True) -> None:
        await self._run("snap", "stop", self.service_name)

    async def restart(self) -> None:
        await self._run("snap", "restart", self.service_name)

    async def follow_output(self) -> AsyncIterator[str]:
        """Yield decoded log lines (utf-8, replace errors)."""
        proc = await self._run(
            "snap", "logs", self.service_name, "-f", to_completion=False
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

    def supports_self_autorestart(self) -> bool:
        return True

    async def exit_code(self) -> Optional[int]:
        status, content = await self._run(
            "systemctl",
            "show",
            "-p",
            "ExecMainStatus",
            "--value",
            f"snap.{self.daemon_service_name}.service",
        )
        if status == 0:
            return int(content)
