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

"""Helper function for spawning the Multipass cli, interactive or non-interactive."""

import sys
import subprocess

from cli.multipass import get_multipass_path
from cli.config import cfg

import pexpect


if sys.platform == "win32":
    from pexpect.popen_spawn import PopenSpawn

    from cli_tests.utilities import WinptySpawn

    class PopenCompatSpawn(PopenSpawn):
        def __init__(self, command, args, **kwargs):
            super().__init__([command, *map(str, args)], **kwargs)

        def isalive(self):
            return self.proc.poll() is None

        def terminate(self, wait=True, kill_on_timeout=True, timeout=5):
            if self.isalive():
                self.proc.terminate()
                if wait:
                    try:
                        self.proc.wait(timeout=timeout)
                    except subprocess.TimeoutExpired:
                        if kill_on_timeout:
                            self.proc.kill()

        def close(self):
            self.terminate()


def spawn_multipass(
    args: list,
    timeout: int | None = None,
    echo: bool = True,
    env: dict | None = None,
    interactive: bool = False,
):
    """Spawn a multipass process with platform-appropriate backend.

    Args:
        args: Arguments to pass to multipass.
        timeout: Process timeout in seconds.
        echo: Echo input (Unix pexpect only).
        env: Environment variables.
        interactive: If True, use WinptySpawn on Windows for interactive sessions.
                     If False, use PopenCompatSpawn for non-interactive usage.
    """
    stream = sys.stdout if sys.platform == "win32" else sys.stdout.buffer
    logfile = stream if cfg.print_cli_output else None

    if sys.platform == "win32":
        if interactive:
            return WinptySpawn(
                [get_multipass_path(), *map(str, args)],
                logfile=logfile,
                timeout=timeout,
                encoding="utf-8",
                codec_errors="replace",
                env=env,
            )
        return PopenCompatSpawn(
            get_multipass_path(),
            [*map(str, args)],
            logfile=logfile,
            timeout=timeout,
            env=env,
        )

    return pexpect.spawn(
        get_multipass_path(),
        [*map(str, args)],
        logfile=logfile,
        timeout=timeout,
        echo=echo,
        env=env,
    )
