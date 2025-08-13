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
import subprocess
import asyncio
from asyncio.subprocess import Process
from contextlib import suppress
from typing import AsyncIterator, Optional

from cli_tests.utilities import send_ctrl_c, get_sudo_tool
from cli_tests.multipass import get_multipass_env, get_multipassd_path


class StandaloneMultipassdController:
    def __init__(self, data_root):
        if not get_multipassd_path():
            raise FileNotFoundError("Could not find 'multipassd' executable!")

        prologue = []
        creationflags = None

        if sys.platform == "win32":
            # No prologue needed -- just pass it via process env
            # The child process needs a new process group and a
            # new console to properly receive the Ctrl-C signal without interfering
            # with the test process itself.
            creationflags = (
                subprocess.CREATE_NEW_PROCESS_GROUP | subprocess.CREATE_NEW_CONSOLE
            )
        else:
            prologue = [*get_sudo_tool(), "env", f"MULTIPASS_STORAGE={data_root}"]

        self._argv = [
            *prologue,
            get_multipassd_path(),
            "--logger=stderr",
            "--verbosity=trace",
        ]
        self._kwargs = {}

        if creationflags is not None:
            self._kwargs["creationflags"] = creationflags

        self._proc: Optional[Process] = None
        self._env = get_multipass_env()

    async def start(self) -> None:
        if self._proc and self._proc.returncode is None:
            raise ChildProcessError("Process not exited yet!")

        self._proc = await asyncio.create_subprocess_exec(
            *self._argv,
            stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.STDOUT,
            env=self._env,
            **self._kwargs,
        )

    async def stop(self, graceful=True) -> None:
        if self._proc is None:
            return

        if not graceful:
            self._proc.kill()
            await self._proc.wait()
            return

        if sys.platform == "win32":
            send_ctrl_c(self._proc.pid)
        else:
            with suppress(ProcessLookupError):
                self._proc.terminate()

        try:
            await asyncio.wait_for(self._proc.wait(), timeout=20)
        except asyncio.TimeoutError:
            with suppress(ProcessLookupError):
                self._proc.kill()
            await self._proc.wait()

    async def restart(self) -> None:
        await self.stop()
        await self.wait_exit()
        await self.start()

    async def follow_output(self) -> AsyncIterator[str]:
        if not self._proc or not self._proc.stdout:
            return

        while True:
            line = await self._proc.stdout.readline()
            if not line:
                # Drain
                await self._proc.communicate()
                break
            yield line.decode("utf-8", "replace")

    async def is_active(self) -> bool:
        return self._proc is not None and self._proc.returncode is None

    async def wait_exit(self) -> Optional[int]:
        if not self._proc:
            return -99

        await self._proc.wait()
        return self._proc.returncode

    async def exit_code(self) -> Optional[int]:
        return self._proc.returncode

    def supports_self_autorestart(self):
        return False
