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
import contextlib
import sys
import re
import os
import tempfile
import plistlib
import subprocess
import logging
from typing import AsyncIterator, Optional

from cli_tests.utilities import get_sudo_tool, run_in_new_interpreter
from .controller_exceptions import ControllerPrerequisiteError

label: str = "com.canonical.multipassd"


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


def get_plist_path() -> Optional[str]:
    # Ask launchd where the job came from.
    rc, out = run_sync("launchctl", "print", f"system/{label}", timeout=10)
    if rc == 0:
        m = re.search(r"^\s*path\s*=\s*(.+)$", out, flags=re.MULTILINE)
        if m:
            return m.group(1).strip()
    # Fallback to canonical location:
    default_path = f"/Library/LaunchDaemons/{label}.plist"
    # We won’t stat here with sudo; launchd will complain on bootstrap if it’s wrong.
    return default_path


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
    data["KeepAlive"] = {"SuccessfulExit": False}  # Mimic Linux systemd behavior

    # Write to a secure temp file; launchd can bootstrap from arbitrary path.
    tmp_dir = tempfile.mkdtemp(prefix="mpd-launchd-")
    tmp_path = os.path.join(tmp_dir, f"{label}.plist")
    with open(tmp_path, "wb") as f:
        plistlib.dump(data, f, sort_keys=False)

    run_in_new_interpreter(make_owner_root_wheel, tmp_path, privileged=True)
    return tmp_path


class LaunchdMultipassdController:
    """Soft launchd controller for multipassd on macOS."""

    @staticmethod
    def setup_environment():
        original_plist_path = get_plist_path()
        logging.info(
            f"LaunchdMultipassdController :: setup_environment {original_plist_path}"
        )

        # 1) Unload whatever is there (may be inactive; ignore failures)
        run_sync("launchctl", "bootout", f"system/{label}")

        # 2) Generate override plist with KeepAlive=false
        if not original_plist_path:
            raise RuntimeError("Cannot determine original plist path for override.")
        override_plist_path = make_override_plist(original_plist_path)

        # 3) Load the override into system domain
        rc, out = run_sync("launchctl", "bootstrap", "system", override_plist_path)
        if rc != 0:
            raise RuntimeError(f"bootstrap(override) failed:\n{out}")

    @staticmethod
    def teardown_environment():
        original_plist_path = get_plist_path()
        logging.info(
            f"LaunchdMultipassdController :: teardown_environment {original_plist_path}"
        )

        # Fully unload override so it doesn’t linger.
        run_sync("launchctl", "bootout", f"system/{label}")
        # Re-bootstrap original plist (restore system “shape”)
        rc, out = run_sync("launchctl", "bootstrap", "system", original_plist_path)
        if rc != 0:
            # Don’t leave the system broken—emit message but keep going.
            print(f"⚠️ restore bootstrap failed:\n{out}")

        rc, out = run_sync("launchctl", "kickstart", f"system/{label}")

    def __init__(self):
        if sys.platform != "darwin":
            raise ControllerPrerequisiteError(
                "LaunchdMultipassdController requires macOS."
            )
        self._sudo = list(get_sudo_tool())
        self._log_proc: Optional[asyncio.subprocess.Process] = None
        self._daemon_pid = None
        self._original_plist_path = None

    async def _run(self, *args: str, timeout: int = 30) -> tuple[int, str]:
        proc = await asyncio.create_subprocess_exec(
            *self._sudo,
            *args,
            stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.STDOUT,
        )
        try:
            await asyncio.wait_for(proc.wait(), timeout=timeout)
        except asyncio.TimeoutError:
            with contextlib.suppress(ProcessLookupError):
                proc.kill()
            await proc.wait()
            raise
        out = (
            (await proc.stdout.read()).decode("utf-8", "replace") if proc.stdout else ""
        )
        return proc.returncode, out

    async def _get_pid(self) -> Optional[int]:
        rc, out = await self._run("launchctl", "print", f"system/{label}", timeout=10)
        if rc != 0:
            return None
        for line in out.splitlines():
            s = line.strip()
            if s.startswith("pid ="):
                try:
                    return int(s.split("=", 1)[1].strip())
                except ValueError:
                    return None
        return None

    async def _wait_inactive(self, timeout_s: float = 20.0) -> bool:
        end = asyncio.get_event_loop().time() + timeout_s
        while asyncio.get_event_loop().time() < end:
            if not await self.is_active():
                return True
            await asyncio.sleep(0.2)
        return False

    # --- DaemonBackend API ---

    async def start(self) -> None:
        # 4) Start (soft): kickstart (no -k)
        rc, out = await self._run("launchctl", "kickstart", f"system/{label}")
        if rc != 0:
            raise RuntimeError(f"kickstart failed:\n{out}")

        self._daemon_pid = await self._get_pid()

    async def stop(self) -> None:
        # Soft stop: TERM via launchctl stop; wait until inactive.
        # KeepAlive may respawn; we’ll wait a bit, then nudge once with kill TERM.
        old_pid = await self._get_pid()
        await self._run("launchctl", "stop", f"system/{label}")

        if await self._wait_inactive(timeout_s=6.0):
            return

        # Still active? If KeepAlive respawned, ask again with a TERM nudge.
        if old_pid is not None:
            await self._run("launchctl", "kill", "SIGTERM", f"system/{label}")
            await self._wait_inactive(timeout_s=6.0)

        # Stop log streaming if active
        if self._log_proc and self._log_proc.returncode is None:
            with contextlib.suppress(ProcessLookupError):
                self._log_proc.terminate()
            with contextlib.suppress(asyncio.TimeoutError):
                await asyncio.wait_for(self._log_proc.wait(), timeout=3)

    async def restart(self) -> None:
        await self.stop()
        # If KeepAlive raced us and it’s already up, kickstart (no -k) is harmless.
        rc, out = await self._run("launchctl", "kickstart", f"system/{label}")
        if rc != 0:
            raise RuntimeError(f"restart (kickstart) failed:\n{out}")

    async def follow_output(self) -> AsyncIterator[str]:
        if self._log_proc and self._log_proc.returncode is None:
            with contextlib.suppress(ProcessLookupError):
                self._log_proc.terminate()
            with contextlib.suppress(asyncio.TimeoutError):
                await asyncio.wait_for(self._log_proc.wait(), timeout=3)

        self._log_proc = await asyncio.create_subprocess_exec(
            *self._sudo,
            "tail",
            "-f",
            "/Library/Logs/Multipass/multipassd.log",
            stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.STDOUT,
        )

        try:
            while True:
                line = await self._log_proc.stdout.readline()
                if not line:
                    break
                yield line.decode("utf-8", "replace")
        except asyncio.CancelledError:
            with contextlib.suppress(ProcessLookupError):
                self._log_proc.terminate()
            with contextlib.suppress(asyncio.TimeoutError):
                await asyncio.wait_for(self._log_proc.wait(), timeout=2)
            raise

    async def is_active(self) -> bool:
        _STATE_RE = re.compile(r"^\s*state\s*=\s*(\w+)", re.M)
        rc, out = await self._run("launchctl", "print", f"system/{label}", timeout=10)
        if rc != 0:
            return False

        m = _STATE_RE.search(out)
        if m:
            return m.group(1).lower() == "running"
        return False

    async def wait_exit(self) -> Optional[int]:
        """Return exit code if available; else None. Should return promptly if stopped."""
        # This will wait for exit.
        while await self.is_active():
            await asyncio.sleep(0.5)
        return await self.exit_code()

    def supports_self_autorestart(self) -> bool:
        return True

    async def wait_for_self_autorestart(self, timeout=60):
        async def _wait():
            current = self._daemon_pid
            while current == self._daemon_pid:
                current = await self._get_pid()
                await asyncio.sleep(0.3)  # polling interval
            self._daemon_pid = current

        await asyncio.wait_for(_wait(), timeout)

    async def exit_code(self) -> Optional[int]:
        """
        Return the last recorded exit code for the launchd job, or None if:
          - the job is currently running, or
          - launchctl doesn't report an exit status.
        """
        rc, out = await self._run("launchctl", "print", f"system/{label}", timeout=10)
        if rc != 0:
            return None

        # If it's running, there is no final exit code yet.
        if re.search(r"^\s*state\s*=\s*running\b", out, flags=re.MULTILINE):
            return None

        # Common fields seen in launchctl output across macOS versions:
        #   "last exit code = <n>"
        #   "last exit status = <n>"     (older variants)
        #   "termination status = <n>"   (seen in some states)
        for pat in (
            r"last exit code\s*=\s*(-?\d+)",
            r"last exit status\s*=\s*(-?\d+)",
            r"termination status\s*=\s*(-?\d+)",
        ):
            m = re.search(pat, out, flags=re.IGNORECASE)
            if m:
                try:
                    return int(m.group(1))
                except ValueError:
                    pass

        return None

    # raise NotImplementedError()
