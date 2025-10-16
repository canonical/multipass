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


import pexpect

if sys.platform == "win32":
    from pexpect.popen_spawn import PopenSpawn
    from cli_tests.utilities import WinptySpawn


from cli_tests.utilities import retry, strip_ansi_escape
from cli_tests.config import config
from cli_tests.multipass import get_multipass_env, get_multipass_path
from cli_tests.multipass.cmd_output import Output


def get_default_timeout_for(cmd):
    default_timeouts = {
        "delete": 90,
        "stop": 180,
        "launch": 600,
        "exec": 90,
        "start": 90,
        "mount": 180,
        "umount": 45,
    }
    if cmd in default_timeouts:
        return default_timeouts[cmd]
    return 10


class ContextManagerProxy:
    """Wrapper to avoid having to type multipass("command")() (note the parens)"""

    def __init__(self, callable):
        self.result = None
        self.callable = callable

    @staticmethod
    def ensure_result(func):
        def wrapper(self, *args, **kwargs):
            if self.result is None:
                self.result = self.callable()
            return func(self, *args, **kwargs)

        return wrapper

    def __enter__(self):
        return self.callable.__enter__()

    def __exit__(self, *a):
        return self.callable.__exit__(*a)

    @ensure_result
    def __getattr__(self, name):
        return getattr(self.result, name)

    @ensure_result
    def __contains__(self, item):
        return self.result.__contains__(item)

    @ensure_result
    def __bool__(self):
        return self.result.__bool__()

    @ensure_result
    def __eq__(self, value):
        return self.result.__eq__(value)

    @ensure_result
    def __repr__(self):
        return strip_ansi_escape(self.result.content)


def multipass(*args, **kwargs):
    """Run a Multipass CLI command with optional retry, timeout, and context manager support.

    This function wraps Multipass CLI invocations using `pexpect`. It supports:
    - Retry logic on failure
    - Interactive vs. non-interactive execution
    - Context manager usage (`with` statement)
    - Lazy evaluation of command output

    Args:
        *args: Positional CLI arguments to pass to the `multipass` command.

    Keyword Args:
        timeout (int, optional): Maximum time in seconds to wait for command completion.Defaults to 30.
        interactive (bool, optional): If True, returns a live interactive `pexpect.spawn` object for manual interaction.
        retry (int, optional): Number of retry attempts on failure. Uses exponential backoff (2s delay).

    Returns:
        - If `interactive=True`: a `pexpect.spawn` object.
        - Otherwise, Output object containing the command's result.

    Raises:
        TimeoutError: If the command execution exceeds the timeout.
        RuntimeError: If the CLI process fails in a non-retryable way.

    Example:
        >>> out = multipass("list", "--format", "json").jq(".instances")
        >>> with multipass("info", "test") as output:
        ...     print(output.exitstatus)

    Notes:
        - You can access output fields directly: `multipass("version").exitstatus`
        - You can use `in` or `bool()` checks on the result proxy.
    """

    timeout = (
        kwargs.get("timeout")
        if "timeout" in kwargs
        else get_default_timeout_for(args[0])
    )
    echo = kwargs.get("echo") or False
    # print(f"timeout is {timeout} for {multipass_path} {' '.join(args)}")
    retry_count = kwargs.pop("retry", None)
    if retry_count is not None:

        @retry(retries=retry_count, delay=2.0)
        def retry_wrapper():
            return multipass(*args, **kwargs)

        return retry_wrapper()

    if config.print_cli_output:
        # Move to the next line since the CLI modifies the current line for
        # updating progress message
        sys.stderr.write("\n")

    if sys.platform == "win32":
        # PopenSpawn is just "good enough" for non-interactive stuff.
        class PopenCompatSpawn(PopenSpawn):
            def __init__(self, command, **kwargs):
                super().__init__(command, **kwargs)
                self.command = command if isinstance(command, list) else command.split()
                print(self.command)

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

    if kwargs.get("interactive"):
        if sys.platform == "win32":
            return WinptySpawn(
                f"{get_multipass_path()} {' '.join(str(arg) for arg in args)}",
                logfile=(sys.stdout if config.print_cli_output else None),
                timeout=timeout,
                encoding="utf-8",
                codec_errors="replace",
                env=get_multipass_env(),
            )

        return pexpect.spawn(
            f"{get_multipass_path()} {' '.join(str(arg) for arg in args)}",
            logfile=(sys.stdout.buffer if config.print_cli_output else None),
            timeout=timeout,
            echo=echo,
            env=get_multipass_env(),
        )

    class Cmd:
        """Run a Multipass CLI command and capture its output.

        Spawns a `pexpect` child process to run the given command,
        waits for it to complete, decodes the output, and ensures cleanup.

        Methods:
            __call__(): Returns an `Output` object with decoded output and exit status.
            __enter__(): Enables use as a context manager, returning the `Output`.
            __exit__(): No-op for context manager exit; always returns False.
        """

        def __init__(self):
            try:
                if not sys.platform == "win32":
                    self.pexpect_child = pexpect.spawn(
                        f"{get_multipass_path()} {' '.join(str(arg) for arg in args)}",
                        logfile=(
                            sys.stdout.buffer if config.print_cli_output else None
                        ),
                        timeout=timeout,
                        echo=echo,
                        env=get_multipass_env(),
                    )
                else:
                    self.pexpect_child = PopenCompatSpawn(
                        [get_multipass_path(), *args],
                        logfile=(
                            sys.stdout.buffer if config.print_cli_output else None
                        ),
                        timeout=timeout,
                        env=get_multipass_env(),
                    )

                self.pexpect_child.expect(pexpect.EOF, timeout=timeout)
                self.output_text = self.pexpect_child.before.decode("utf-8")
                self.pexpect_child.wait()
            except Exception as ex:
                print(ex)
                raise
            finally:
                if self.pexpect_child.isalive():
                    sys.stderr.write(
                        f"\n❌🔥 Terminating {get_multipass_path()} {' '.join(args)}\n"
                    )
                    sys.stdout.flush()
                    sys.stderr.flush()
                    self.pexpect_child.terminate()

        def __call__(self):
            return Output(self.output_text, self.pexpect_child.exitstatus)

        def __enter__(self):
            return self.__call__()

        def __exit__(self, *args):
            return False

    cmd = Cmd()
    return ContextManagerProxy(cmd)
