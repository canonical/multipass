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

from __future__ import annotations
import asyncio
import sys
import re
import os
import plistlib
import subprocess
import logging
from typing import AsyncIterator, Optional
from contextlib import suppress

from cli_tests.utilities import (
    get_sudo_tool,
    run_in_new_interpreter,
    StdoutAsyncSubprocess,
    SilentAsyncSubprocess,
    sudo,
    TempDirectory,
)
from .controller_exceptions import ControllerPrerequisiteError

label: str = "com.canonical.multipassd"
plist_path = f"/Library/LaunchDaemons/{label}.plist"


def run_sync(*args: str, timeout: int = 30) -> tuple[int, str]:
    try:
        proc = subprocess.run(
            [*get_sudo_tool(), *args],
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            timeout=timeout,
            check=False,
        )
        return proc.returncode, proc.stdout.decode("utf-8", "replace")
    except subprocess.TimeoutExpired as exc:
        return -1, exc.output.decode("utf-8", "replace") if exc.output else ""


def make_owner_root_wheel(path):
    import pwd, grp, os

    root_uid = pwd.getpwnam("root").pw_uid
    wheel_gid = grp.getgrnam("wheel").gr_gid
    os.chown(path, root_uid, wheel_gid)
    os.chmod(path, 0o644)


def make_override_plist(source_plist_path: str) -> str:
    # Read original plist (root-readable). We don’t sudo-read here; rely on world-readability.
    with open(source_plist_path, "rb") as f:
        data = plistlib.load(f)

    # Normalize label and ensure KeepAlive disabled.
    data["Label"] = label
    # Remove any complex KeepAlive dicts and force boolean False.
    data["KeepAlive"] = False

    with TempDirectory(delete=False) as tmp_dir:
        tmp_path = os.path.join(tmp_dir, f"{label}.plist")
        with open(tmp_path, "wb") as f:
            plistlib.dump(data, f, sort_keys=False)

    run_in_new_interpreter(make_owner_root_wheel, tmp_path, privileged=True)

    return tmp_path


class LaunchdMultipassdController:
    """Soft launchd controller for multipassd on macOS."""

    @staticmethod
    def setup_environment():
        logging.debug(f"LaunchdMultipassdController :: setup_environment {plist_path}")

        # 1) Unload whatever is there (may be inactive; ignore failures)
        run_sync("launchctl", "bootout", f"system/{label}")

        override_plist_path = make_override_plist(plist_path)

        # 3) Load the override into system domain
        rc, out = run_sync("launchctl", "bootstrap", "system", override_plist_path)
        if rc != 0:
            raise RuntimeError(f"bootstrap(override) failed:\n{out}")

    @staticmethod
    def teardown_environment():
        logging.debug(
            f"LaunchdMultipassdController :: teardown_environment {plist_path}"
        )

        # Fully unload override so it doesn’t linger.
        run_sync("launchctl", "bootout", f"system/{label}")

        # Re-bootstrap original plist (restore system “shape”)
        rc, out = run_sync("launchctl", "bootstrap", "system", plist_path)
        if rc != 0:
            # Don’t leave the system broken—emit message but keep going.
            print(f"⚠️ restore bootstrap failed:\n{out}")

        rc, out = run_sync("launchctl", "kickstart", f"system/{label}")

    def __init__(self):
        if sys.platform != "darwin":
            raise ControllerPrerequisiteError(
                "LaunchdMultipassdController requires macOS."
            )
        self._log_proc: Optional[asyncio.subprocess.Process] = None
        self._daemon_pid = None

    async def _get_service_property(self, prop_name) -> str:
        _regex = re.compile(rf"^\s*{prop_name}\s*=\s*(.*)$", re.M)

        async with StdoutAsyncSubprocess(
            "launchctl", "print", f"system/{label}"
        ) as proc:
            stdout, _ = await proc.communicate()
            if proc.returncode != 0:
                return False

            m = _regex.search(stdout.decode(encoding="utf-8", errors="replace"))
            if m:
                return m.group(1).strip()

        return None

    async def _get_pid(self) -> Optional[int]:
        prop = await self._get_service_property("pid")
        if prop:
            return int(prop.strip())
        return None

    # --- DaemonBackend API ---

    async def start(self) -> None:
        # 4) Start (soft): kickstart (no -k)
        async with StdoutAsyncSubprocess(
            *sudo("launchctl", "kickstart", f"system/{label}")
        ) as start:
            stdout, _ = await start.communicate()
            if start.returncode == 0:
                self._daemon_pid = await self._get_pid()
                return
            raise RuntimeError(f"kickstart failed:\n{stdout}")

        self._daemon_pid = await self._get_pid()

    async def stop(self) -> None:
        async with SilentAsyncSubprocess(
            "launchctl", "stop", f"system/{label}"
        ) as stop:
            await stop.communicate()

        with suppress(asyncio.TimeoutError):
            if await asyncio.wait_for(self.wait_exit(), 5):
                return

        async with SilentAsyncSubprocess(
            "launchctl", "kill", "SIGTERM", f"system/{label}"
        ) as kill:
            await kill.communicate()

        await asyncio.wait_for(self.wait_exit(), 10)

    async def restart(self) -> None:
        await self.stop()

        async with StdoutAsyncSubprocess(
            *sudo("launchctl", "kickstart", f"system/{label}")
        ) as start:
            stdout, _ = await start.communicate()
            if start.returncode == 0:
                return
            raise RuntimeError(f"restart (kickstart) failed:\n{stdout}")

    async def follow_output(self) -> AsyncIterator[str]:
        async with StdoutAsyncSubprocess(
            *sudo(
                "tail",
                "-f",
                "/Library/Logs/Multipass/multipassd.log",
            )
        ) as tail:
            while True:
                line = await tail.stdout.readline()
                if not line:
                    break
                yield line.decode("utf-8", "replace")

    async def is_active(self) -> bool:
        prop = await self._get_service_property("state")
        if not prop:
            return False

        return prop.lower() == "running"

    async def wait_exit(self) -> Optional[int]:
        """Return exit code if available; else None. Should return promptly if stopped."""
        # This will wait for exit.
        while await self.is_active():
            await asyncio.sleep(0.5)
        return await self.exit_code()

    def supports_self_autorestart(self) -> bool:
        return False

    # async def wait_for_self_autorestart(self, timeout=60):
    #     async def _wait():
    #         current = self._daemon_pid
    #         while current == self._daemon_pid:
    #             current = await self._get_pid()
    #             await asyncio.sleep(0.3)  # polling interval
    #         self._daemon_pid = current

    #     await asyncio.wait_for(_wait(), timeout)

    async def exit_code(self) -> Optional[int]:
        """
        Return the last recorded exit code for the launchd job, or None if:
          - the job is currently running, or
          - launchctl doesn't report an exit status.
        """

        if await self.is_active():
            return None

        props_to_look_for = [
            "last exit code",
            "last exit status",
            "termination status",
        ]

        for prop_name in props_to_look_for:
            try:
                result = await self._get_service_property(prop_name)
                if result == "(never exited)":
                    return 0
                return int(result)
            except ValueError:
                pass

        return None
