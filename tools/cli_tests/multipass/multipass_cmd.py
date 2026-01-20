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

"""Wrapper for running Multipass CLI commands in tests with retry, timeouts, and interactive support."""

import subprocess
import sys
import logging
import uuid


import pexpect

from cli_tests.config import cfg
from cli_tests.multipass import get_multipass_env, get_multipass_path
from cli_tests.multipass.cmd_output import Output
from cli_tests.utilities import retry, wrap_call_if

if sys.platform == "win32":
    from pexpect.popen_spawn import PopenSpawn

    from cli_tests.utilities import WinptySpawn


def non_option_args(args):
    return [a for a in args if isinstance(a, str) and not a.startswith('-')]


def is_valid_uuid(in_str):
    try:
        uuid.UUID(in_str)
        return True
    except (ValueError, TypeError):
        return False


def _restart_hook_common(args):
    filtered_args = non_option_args(args)
    assert len(filtered_args) >= 2
    # Remove "restart" from the filtered args
    assert filtered_args.pop(0) == "restart"
    return filtered_args


def get_boot_id(name):
    """
    Get boot ID from VM.
    """
    with multipass(
        "exec", name, "--", "cat", "/proc/sys/kernel/random/boot_id"
    ) as result:
        assert result, f"Failed: {result.content} ({result.exitstatus})"
        uuid_str = str(result).strip()
        assert is_valid_uuid(uuid_str)
        return uuid_str


def restart_prologue(args, kwargs):
    logging.debug(f"restart_prologue: args: {args}, kwargs: {kwargs}")
    filtered_args = _restart_hook_common(args)

    boot_ids = {}
    for vm_name in filtered_args:
        boot_ids[vm_name] = get_boot_id(vm_name)

    return boot_ids


def restart_epilogue(args, kwargs, result, prologue_result):
    logging.debug(
        f"epilogue: args: {args}, kwargs: {kwargs}, result: {result}, prologue_result: {prologue_result}")
    filtered_args = _restart_hook_common(args)
    for vm_name in filtered_args:
        assert vm_name in prologue_result

        @retry(retries=50, delay=20)
        def attempt_ssh(vm_name):
            with multipass("exec", vm_name, "--", "echo", "ok") as v:
                boot_id = get_boot_id(vm_name)
                v.exitstatus = 0 if boot_id != prologue_result[vm_name] else 1
                if v.exitstatus == 1:
                    logging.debug(
                        f"prev boot id {prologue_result[vm_name]} matches the current {boot_id}, VM hasn't restarted yet.")
                return v

        assert attempt_ssh(vm_name)

@wrap_call_if("restart", restart_prologue, restart_epilogue)
def multipass(*args, **kwargs):
    """Run a Multipass CLI command with optional retry, timeout, and context
    manager support.

    This function wraps Multipass CLI invocations using `pexpect`. It supports:

    - Retry logic on failure
    - Interactive vs. non-interactive execution
    - Context manager usage (`with` statement)
    - Lazy evaluation of command output

    Args:
        *args: Positional CLI arguments to pass to the `multipass` command.

    Keyword Args:
        timeout (int, optional): Maximum time in seconds to wait for command
        completion.Defaults to 30.
        interactive (bool, optional): If True, returns a live interactive
        `pexpect.spawn` object for manual interaction.
        retry (int, optional): Number of retry attempts on failure. Uses
        exponential backoff (2s delay).

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

    DEFAULT_CMD_TIMEOUT = 30

    timeout = (
        kwargs.get("timeout")
        if "timeout" in kwargs
        else getattr(cfg.timeouts, args[0], DEFAULT_CMD_TIMEOUT)
    )
    echo = kwargs.get("echo") or False
    retry_count = kwargs.get("retry", getattr(cfg.retries, args[0], None))

    if retry_count and retry_count > 0:

        @retry(retries=retry_count, delay=2.0)
        def retry_wrapper():
            # Pass retry "0" to prevent further recursion
            return multipass(*args, **{**kwargs, "retry": 0})

        return retry_wrapper()

    if cfg.print_cli_output:
        # Move to the next line since the CLI modifies the current line for
        # updating progress message
        sys.stderr.write("\n")

    if sys.platform == "win32":
        # PopenSpawn is just "good enough" for non-interactive stuff.
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

    env = get_multipass_env()
    env_args = kwargs.get("env")

    if env_args:
        env.update(env_args)

    cmd_args = [get_multipass_path(), *map(str, args)]
    logging.debug(f"cmd: {cmd_args}")

    if kwargs.get("interactive"):
        if sys.platform == "win32":
            return WinptySpawn(
                [get_multipass_path(), *map(str, args)],
                logfile=(sys.stdout if cfg.print_cli_output else None),
                timeout=timeout,
                encoding="utf-8",
                codec_errors="replace",
                env=env,
            )

        return pexpect.spawn(
            get_multipass_path(),
            [*map(str, args)],
            logfile=(sys.stdout.buffer if cfg.print_cli_output else None),
            timeout=timeout,
            echo=echo,
            env=env,
        )

    class RunToCompletion:
        """Run the cli command and capture its output.

        Spawns a `pexpect` child process to run the given command,
        waits for it to complete, decodes the output, and ensures cleanup.

        Methods:
            __call__(): Returns an `Output` object with decoded output and exit status.
            __enter__(): Enables use as a context manager, returning the `Output`.
            __exit__(): No-op for context manager exit; always returns False.
        """

        def __init__(self):
            self.pexpect_child = None
            try:
                if sys.platform != "win32":
                    self.pexpect_child = pexpect.spawn(
                        get_multipass_path(),
                        [*map(str, args)],
                        logfile=(
                            sys.stdout.buffer if cfg.print_cli_output else None
                        ),
                        timeout=timeout,
                        echo=echo,
                        env=env,
                    )
                else:
                    self.pexpect_child = PopenCompatSpawn(
                        get_multipass_path(),
                        [*map(str, args)],
                        logfile=(
                            sys.stdout.buffer if cfg.print_cli_output else None
                        ),
                        timeout=timeout,
                        env=env,
                    )

                self.pexpect_child.expect(pexpect.EOF, timeout=timeout)
                self.output_text = self.pexpect_child.before.decode("utf-8")
                self.pexpect_child.wait()
            except Exception as ex:
                print(ex)
                raise
            finally:
                if self.pexpect_child and self.pexpect_child.isalive():
                    sys.stderr.write(
                        f"\n❌🔥 Terminating {' '.join(cmd_args)}\n")
                    sys.stdout.flush()
                    sys.stderr.flush()
                    self.pexpect_child.terminate()

        def __call__(self):
            return Output(
                self.output_text,
                self.pexpect_child.exitstatus,
            )

        def __enter__(self):
            return self.__call__()

        def __exit__(self, *args):
            return False

    cmd = RunToCompletion()
    return cmd()
