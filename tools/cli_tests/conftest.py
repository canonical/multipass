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

import pytest

from cli_tests.utils import Output


def pytest_addoption(parser):
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
        help="Do not automatically launch the daemon for the tests.",
    )

    parser.addoption(
        "--remove-all-instances",
        action="store_true",
        help="Remove all instances from `data-root` before starting testing.",
    )


def pytest_configure(config):
    if config.getoption("--build-root") is None:
        pytest.exit("ERROR: The `--build-root` argument is required.", returncode=1)


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


@pytest.fixture(scope="session", autouse=True)
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

        # Need to authenticate
        sys.stdout.write("\n\n")
        sys.stdout.flush()
        subprocess.run(["sudo", "-v"], check=True)
    except subprocess.TimeoutExpired:
        pytest.skip("Cannot authenticate sudo non-interactively")


@pytest.fixture(scope="session")
def data_root(request):
    # If user gave --data-root, use it
    user_value = request.config.getoption("--data-root")
    if user_value:
        return user_value
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
            print(f"\n🧹 ⚠️ Permission denied, escalating to sudo rm -rf {tmpdir.name}")
            subprocess.run(["sudo", "rm", "-rf", tmpdir.name], check=True)

    # Register finalizer to cleanup on exit
    request.addfinalizer(cleanup)
    return tmpdir.name


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


@pytest.fixture
def multipass(request):
    """Convenience wrapper for Multipass CLI"""
    multipass_path = shutil.which(
        "multipass", path=request.config.getoption("--build-root")
    )
    return lambda *args, **kwargs: subprocess.Popen(
        [multipass_path] + list(args),
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        **kwargs,
    )


def die(exit_code, message):
    """End testing process abruptly."""
    sys.stderr.write(f"\n❌🔥 {message}\n")
    # Flush both streams
    sys.stdout.flush()
    sys.stderr.flush()
    os._exit(exit_code)


def wait_for_multipassd_ready(cli, timeout=10):
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
        shell_proc = cli("shell", nonexistent_instance_name)
        out, _ = shell_proc.communicate(timeout=5)

        # The daemon will respond with "instance <name> not running" when it's up"
        if re.search(f".*{nonexistent_instance_name}.*", out):
            # Daemon is up and responding
            # Also verify the version.
            version_out, _ = cli("version").communicate(timeout=5)
            version_out = version_out.splitlines() if version_out else []

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

        time.sleep(0.2)

    sys.stderr.write("\n❌ Fatal error: The daemon did not respond in time!\n")
    sys.stderr.flush()
    return False
    # os._exit(3)

@pytest.fixture
def multipassd(request, multipass, data_root):

    if request.config.getoption("--no-daemon") is False:
        sys.stdout.write("Skipping launching the daemon.")
        yield None
        return

    multipassd_path = shutil.which(
        "multipassd", path=request.config.getoption("--build-root")
    )
    if not multipassd_path:
        die(2, "Could not find 'multipassd' executable!")

    multipassd_env = os.environ.copy()
    multipassd_env["MULTIPASS_STORAGE"] = data_root

    if request.config.getoption("--remove-all-instances"):
        instance_records_file = os.path.join(
            data_root, "data/vault/multipassd-instance-image-records.json"
        )
        subprocess.run(
            ["sudo", "truncate", "-s", "0", instance_records_file], check=True
        )
        subprocess.run(
            ["sudo", "rm", "-rf", os.path.join(data_root, "data/vault/instances")],
            check=True,
        )

    prologue = []
    if sys.platform == "win32":
        pass
    else:
        prologue = ["sudo", "env", f"MULTIPASS_STORAGE={data_root}"]

    with run_process(
        prologue + [multipassd_path, "--logger=stderr"], env=multipassd_env
    ) as multipassd_proc:
        test_scope_exit = False

        def monitor():
            poll_interval = 0.5
            print_daemon_output = request.config.getoption("--print-daemon-output")
            while True:
                if multipassd_proc.poll() is not None:

                    # Graceful exit.
                    if test_scope_exit and multipassd_proc.returncode == 0:
                        return

                    sys.stderr.write(
                        f"\n💥 FATAL: multipassd died with code {multipassd_proc.returncode}\n"
                    )
                    sys.stderr.flush()

                    reason_dict = {
                        r".*dnsmasq: failed to create listening socket.*": "Could not bind dnsmasq to port 53, is there another process running?"
                    }
                    reasons = []

                    for line in multipassd_proc.stdout:
                        if print_daemon_output:
                            print(line, end="")
                        for k, v in reason_dict.items():
                            if re.search(k, line):
                                reasons.append(f"\nReason: {v}")

                    die(
                        5,
                        f"FATAL: multipassd died with code {multipassd_proc.returncode}!\n"
                        "\n".join(reasons),
                    )
                if print_daemon_output:
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

        if wait_for_multipassd_ready(multipass) is False:
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
