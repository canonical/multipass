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
import os
import re
import subprocess
import shutil
import threading
import time
import tempfile
import select
from contextlib import contextmanager
from types import SimpleNamespace

import pexpect
import pytest

from cli_tests.utils import Output, retry

config = SimpleNamespace()


def pytest_addoption(parser):
    """Populate command line args for the CLI tests."""
    parser.addoption(
        "--build-root",
        action="store",
        help="Build root directory for multipass.",
    )

    parser.addoption(
        "--data-root",
        action="store",
        help="Data root directory for multipass.",
    )

    parser.addoption(
        "--no-daemon",
        action="store_false",
        help="Do not automatically launch the daemon for the tests.",
    )

    parser.addoption(
        "--print-daemon-output",
        action="store_true",
        help="Print daemon output to stdout",
    )

    parser.addoption(
        "--print-cli-output",
        action="store_true",
        help="Print CLI output to stdout",
    )

    parser.addoption(
        "--remove-all-instances",
        action="store_true",
        help="Remove all instances from `data-root` before starting testing.",
    )


def pytest_configure(config):
    """Validate command line args."""
    if config.getoption("--build-root") is None:
        pytest.exit("ERROR: The `--build-root` argument is required.", returncode=1)


@pytest.fixture(autouse=True)
def store_config(request):
    """Store the given command line args in a global variable so
    they would be accessible to all functions in the module."""
    config.build_root = request.config.getoption("--build-root")
    config.data_root = request.config.getoption("--data-root")
    config.no_daemon = request.config.getoption("--no-daemon")
    config.print_daemon_output = request.config.getoption("--print-daemon-output")
    config.print_cli_output = request.config.getoption("--print-cli-output")
    config.remove_all_instances = request.config.getoption("--remove-all-instances")

    # If user gave --data-root, use it
    if not config.data_root:
        # Otherwise, create a temp dir for the whole session
        tmpdir = tempfile.TemporaryDirectory()

        def cleanup():
            """Since the daemon owns the data_root on startup
            a non-root user cannot remove it. Therefore, try
            privilege escalation if nuking as normal fails."""
            try:
                shutil.rmtree(tmpdir.name)
                print(f"\n🧹 ✅ Cleaned up {tmpdir.name} normally.")
            except PermissionError:
                print(
                    f"\n🧹 ⚠️ Permission denied, escalating to sudo rm -rf {tmpdir.name}"
                )
                subprocess.run(["sudo", "rm", "-rf", tmpdir.name], check=True)

        # Register finalizer to cleanup on exit
        request.addfinalizer(cleanup)
        config.data_root = tmpdir.name


def pytest_assertrepr_compare(op, left, right):
    """Custom assert pretty-printer for `"pattern" in Output()`"""
    if isinstance(right, Output) and op == "in":
        lines = [
            f"❌ A text matching the pattern `{left}` not found in actual CLI command output:",
            "",  # Empty line for spacing
        ]
        # Split the content by lines and add each as a separate list item
        content_lines = right.content.split("\n")
        lines.extend(content_lines)
        lines.append("")  # Empty line at the end

        return lines
    return None


def die(exit_code, message):
    """End testing process abruptly."""
    sys.stderr.write(f"\n❌🔥 {message}\n")
    # Flush both streams
    sys.stdout.flush()
    sys.stderr.flush()
    os._exit(exit_code)


@pytest.fixture(autouse=True, scope="session")
def ensure_sudo_auth():
    """Ensure sudo is authenticated before running tests"""
    if sys.platform == "win32":
        # TODO: Ensure admin?
        return  # Skip on Windows
    try:
        # Test if sudo is already authenticated
        result = subprocess.run(
            ["sudo", "-n", "true"], capture_output=True, timeout=1, check=False
        )

        if result.returncode == 0:
            return

        message = (
            "🔒🔒🔒🔒🔒🔒🔒🔒🔒🔒🔒🔒🔒🔒🔒🔒🔒🔒🔒🔒🔒🔒🔒🔒🔒\n"
            "Multipass CLI tests require `sudo` authentication.\n"
            "This only needs to be done once per session.\n"
            "You'll be prompted to enter your password now.\n"
            "🔒🔒🔒🔒🔒🔒🔒🔒🔒🔒🔒🔒🔒🔒🔒🔒🔒🔒🔒🔒🔒🔒🔒🔒🔒\n"
        )

        sys.stdout.write(f"\n\n{message}")
        sys.stdout.flush()
        subprocess.run(["sudo", "-v"], check=True)
    except subprocess.TimeoutExpired:
        pytest.skip("Cannot authenticate sudo non-interactively")


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
    multipass_path = shutil.which("multipass", path=config.build_root)

    timeout = kwargs.get("timeout") if "timeout" in kwargs else 30
    # print(f"timeout is {timeout} for {multipass_path} {' '.join(args)}")
    retry_count = kwargs.pop("retry", None)
    if retry_count is not None:

        @retry(retries=retry_count, delay=2.0)
        def retry_wrapper():
            return multipass(*args, **kwargs)

        return retry_wrapper()

    if kwargs.get("interactive"):
        return pexpect.spawn(
            f"{multipass_path} {' '.join(args)}",
            logfile=(sys.stdout.buffer if config.print_cli_output else None),
            timeout=timeout,
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
                self.pexpect_child = pexpect.spawn(
                    f"{multipass_path} {' '.join(args)}",
                    logfile=(sys.stdout.buffer if config.print_cli_output else None),
                    timeout=timeout,
                )
                self.pexpect_child.expect(pexpect.EOF, timeout=timeout)
                self.pexpect_child.wait()
                self.output_text = self.pexpect_child.before.decode("utf-8")
            finally:
                if self.pexpect_child.isalive():
                    sys.stderr.write(
                        f"\n❌🔥 Terminating {multipass_path} {' '.join(args)}\n"
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

    class ContextManagerProxy:
        """Wrapper to avoid having to type multipass("command")() (note the parens)"""

        def __init__(self):
            self.result = None

        def _populate_result(self):
            if self.result is None:
                self.result = cmd()

        def __getattr__(self, name):
            # Lazily run the command if someone tries to access attributes
            self._populate_result()
            return getattr(self.result, name)

        def __enter__(self):
            return cmd.__enter__()

        def __exit__(self, *a):
            return cmd.__exit__(*a)

        def __contains__(self, item):
            self._populate_result()
            return self.result.__contains__(item)

        def __bool__(self):
            self._populate_result()
            return self.result.__bool__()

    return ContextManagerProxy()


def wait_for_multipassd_ready(timeout=10):
    """
    Wait until Multipass daemon starts responding to the CLI commands.
    For that, the function tries to shell into a non-existent instance.
    The daemon is expected to respond with "instance does not exists"
    when it's up and running.

    When the daemon starts, the function also verifies that the daemon's
    version matches the CLI's version.

    If any of these expectancies fail, it's a hard failure and it'll cause
    the test executable to exit with an error code.
    """
    deadline = time.time() + timeout
    while time.time() < deadline:
        # The function  uses `shell` command to check if daemon is ready.
        # An UUID is good enough to ensure that it does not exist.
        nonexistent_instance_name = "6b76819d-faa8-404b-a65a-2183e5fe2cb6"
        pattern = re.compile(f".*{nonexistent_instance_name}.*")
        try:
            with multipass(
                "shell", nonexistent_instance_name, timeout=5, interactive=True
            ) as shell:
                shell.expect(pattern)
            with multipass("version") as version:
                version_out = version.content.splitlines() if version.content else []
                # One line for multipass, one line for multipassd.
                if len(version_out) == 2:
                    cli_ver = version_out[0].split()[-1]
                    daemon_ver = version_out[1].split()[-1]
                    if cli_ver == daemon_ver:
                        return True  # No issues.

                    sys.stderr.write(
                        f"Version mismatch detected!\n"
                        f"👉 CLI version: {cli_ver}\n"
                        f"👉 Daemon version: {daemon_ver}\n"
                    )
                    sys.stderr.flush()
                    return False
        except pexpect.exceptions.EOF:
            pass
        except pexpect.exceptions.TIMEOUT:
            pass

        time.sleep(0.2)

    sys.stderr.write("\n❌ Fatal error: The daemon did not respond in time!\n")
    sys.stderr.flush()
    return False


@pytest.fixture(autouse=True)
def multipassd(store_config):
    """Automatically manage the lifecycle of the multipassd daemon for tests.

    - Skips daemon launch if `config.no_daemon` is False.
    - Locates and runs `multipassd` from `config.build_root`.
    - Optionally purges instance state if `config.remove_all_instances` is set.
    - Starts `multipassd` in a subprocess with a watchdog thread that:
        - Monitors output for known failure patterns (e.g., dnsmasq port conflict)
        - Streams logs if `config.print_daemon_output` is enabled
    - Waits for the daemon to become ready before tests run.
    - Ensures clean shutdown on fixture teardown.

    Yields:
        subprocess.Popen: The running `multipassd` process handle, or None if skipped.

    Raises:
        SystemExit: If prerequisites are unmet, the daemon dies early, or readiness check fails.
    """

    if config.no_daemon is False:
        sys.stdout.write("Skipping launching the daemon.")
        yield None
        return

    multipassd_path = shutil.which("multipassd", path=config.build_root)
    if not multipassd_path:
        die(2, "Could not find 'multipassd' executable!")

    multipassd_env = os.environ.copy()
    multipassd_env["MULTIPASS_STORAGE"] = config.data_root

    if config.remove_all_instances:
        sys.stderr.flush()
        sys.stdout.flush()
        instance_records_file = os.path.join(
            config.data_root, "data/vault/multipassd-instance-image-records.json"
        )
        # FIXME: Cross-platform
        subprocess.run(
            ["sudo", "truncate", "-s", "0", instance_records_file], check=False
        )
        subprocess.run(
            [
                "sudo",
                "rm",
                "-rf",
                os.path.join(config.data_root, "data/vault/instances"),
            ],
            check=False,
        )

    prologue = []
    if sys.platform == "win32":
        pass
    else:
        prologue = ["sudo", "env", f"MULTIPASS_STORAGE={config.data_root}"]

    @contextmanager
    def run_process(cmd, **kwargs):
        """Execute and yield a process."""
        proc = subprocess.Popen(
            cmd, **kwargs, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True
        )
        try:
            yield proc
        finally:
            proc.terminate()
            try:
                proc.wait(timeout=30)
            except subprocess.TimeoutExpired:
                proc.kill()

    with run_process(
        prologue + [multipassd_path, "--logger=stderr"], env=multipassd_env
    ) as multipassd_proc:
        test_scope_exit = False

        def monitor():
            poll_interval = 0.5
            while True:
                if multipassd_proc.poll() is not None:

                    # Graceful exit.
                    if test_scope_exit and multipassd_proc.returncode == 0:
                        return

                    reason_dict = {
                        r".*dnsmasq: failed to create listening socket.*": "Could not bind dnsmasq to port 53, is there another process running?"
                    }
                    reasons = []

                    for line in multipassd_proc.stdout:
                        if config.print_daemon_output:
                            print(line, end="")
                        for k, v in reason_dict.items():
                            if re.search(k, line):
                                reasons.append(f"\nReason: {v}")

                    die(
                        5,
                        f"FATAL: multipassd died with code {multipassd_proc.returncode}!\n"
                        "\n".join(reasons),
                    )
                if config.print_daemon_output:
                    rlist, _, _ = select.select(
                        [multipassd_proc.stdout], [], [], poll_interval
                    )
                    for fd in rlist:
                        line = fd.readline()
                        if not line:
                            continue  # EOF? maybe handle
                        print(line, end="")
                time.sleep(poll_interval)

        # Spawn a watchdog thread to monitor multipassd's status
        monitor_thread = threading.Thread(target=monitor, daemon=True)
        monitor_thread.start()

        if wait_for_multipassd_ready() is False:
            print("⚠️ multipassd not ready, attempting graceful shutdown...")
            multipassd_proc.terminate()  # polite SIGTERM
            try:
                multipassd_proc.wait(timeout=5)  # wait up to 5 sec
            except subprocess.TimeoutExpired:
                print("⏰ Graceful shutdown failed, killing forcefully...")
                multipassd_proc.kill()  # no mercy
                multipassd_proc.wait()  # ensure it's reaped
            die(3, "Tests cannot proceed, daemon not responding in time.")

        yield multipassd_proc
        test_scope_exit = True
