# pylint: disable=broad-exception-caught
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

"""Pytest customizations."""

import sys
import os
import subprocess
import shutil
import tempfile
import asyncio
import threading
import logging
from pathlib import Path

import pytest

from cli_tests.utils import (
    Output,
    multipass,
    uuid4_str,
    mounts,
    state,
    get_sudo_tool,
    die,
    wait_for_future
)
from cli_tests.config import config
from cli_tests.async_multipassd_controller import AsyncMultipassdController


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
    logging.basicConfig(
        level=logging.INFO,
        format="[%(levelname)s] %(message)s"
    )
    if config.getoption("--build-root") is None:
        pytest.exit("ERROR: The `--build-root` argument is required.", returncode=1)

def pytest_assertrepr_compare(op, left, right):
    """Custom assert pretty-printer for `"pattern" in Output()`"""

    print(f"pytest_assertrepr_compare > {op} {left} {right}")
    from cli_tests.utils import ContextManagerProxy
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

@pytest.fixture(autouse=True, scope="session")
def ensure_sudo_auth():
    """Ensure sudo is authenticated before running tests"""
    try:
        # Test if sudo is already authenticated
        result = subprocess.run(
            [*get_sudo_tool(), "-n", "true"], capture_output=True, timeout=1, check=False
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
        subprocess.run([get_sudo_tool(), "-v"], check=True)
    except subprocess.TimeoutExpired:
        pytest.skip("Cannot authenticate sudo non-interactively")

def privileged_truncate_file(target, check=False):
    subprocess.run(
        [
            *get_sudo_tool(),
            sys.executable,
            "-c",
            "import sys; open(sys.argv[1], 'w').close()",
            str(target),
        ],
        check=check,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )


def privileged_remove_path(target, check=False):
    subprocess.run(
        [
            *get_sudo_tool(),
            sys.executable,
            "-c",
            "import shutil; import sys; shutil.rmtree(sys.argv[1], ignore_errors=True)",
            str(Path(target)),
        ],
        check=check,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )

class BackgroundEventLoop:
    def __init__(self):
        self.loop = asyncio.new_event_loop()
        self.thread = threading.Thread(target=self._run_loop, name="asyncio-loop-thr")
        self.thread.start()

    def _run_loop(self):
        asyncio.set_event_loop(self.loop)
        self.loop.run_forever()

    def run(self, coro):
        """Submit coroutine to background loop"""
        return asyncio.run_coroutine_threadsafe(coro, self.loop)

    def stop(self):
        self.loop.call_soon_threadsafe(self.loop.stop)
        self.thread.join()

@pytest.fixture(autouse=True)
def multipassd(store_config):

    if config.no_daemon is False:
        sys.stdout.write("Skipping launching the daemon.")
        yield None
        return

    if config.remove_all_instances:
        sys.stderr.flush()
        sys.stdout.flush()
        instance_records_file = os.path.join(
            config.data_root, "data/vault/multipassd-instance-image-records.json"
        )
        instances_dir = os.path.join(config.data_root, "data/vault/instances")
        privileged_truncate_file(instance_records_file)
        privileged_remove_path(instances_dir)

    controller = None
    bg_loop = BackgroundEventLoop()

    try:
        controller = AsyncMultipassdController(
            bg_loop, config.build_root, config.data_root, config.print_daemon_output
        )
        wait_for_future(bg_loop.run(controller.start()))
        yield controller
    except Exception as exc:
        die(2, str(exc))
    finally:
        if controller:
            wait_for_future(bg_loop.run(controller.stop()))
        sys.stderr.flush()
        sys.stdout.flush()
        bg_loop.stop()

@pytest.fixture
def windows_privileged_mounts(multipassd):
    if sys.platform == "win32":
        # Check if privileged mounts are already enabled
        # if "true" in multipass("get", "local.privileged-mounts"):
        #     return
        multipassd.autorestart(1)
        assert multipass("set", "local.privileged-mounts=1")

@pytest.fixture(scope="function")
def instance():
    """Launch a VM and ensure cleanup."""
    name = uuid4_str("instance")
    assert multipass(
        "launch",
        "--cpus",
        "2",
        "--memory",
        "1G",
        "--disk",
        "6G",
        "--name",
        name,
        retry=3,
    )
    assert mounts(name) == {}
    assert state(name) == "Running"
    yield name
    assert multipass("delete", name, "--purge")
