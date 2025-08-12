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
import subprocess
import shutil
import logging
from contextlib import contextmanager


import pytest

from cli_tests.utilities import (
    wait_for_future,
    get_sudo_tool,
    run_as_subprocess,
    BackgroundEventLoop,
    TempDirectory,
)

from cli_tests.multipass import (
    Output,
    multipass,
    default_driver_name,
    launch,
    nuke_all_instances,
    SNAP_MULTIPASSD_STORAGE,
    SNAP_BIN_DIR,
)

from cli_tests.config import config
from cli_tests.controller import (
    MultipassdGovernor,
    SnapdMultipassdController,
    StandaloneMultipassdController,
)


def pytest_addoption(parser):
    """Populate command line args for the CLI tests."""
    parser.addoption(
        "--bin-dir",
        action="store",
        help="Directory to look for the multipass binaries.",
    )

    parser.addoption(
        "--data-root",
        action="store",
        help="Data root directory for multipass.",
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

    parser.addoption(
        "--driver",
        default=default_driver_name(),
        help="Backend to use.",
    )

    parser.addoption(
        "--daemon-controller",
        default="standalone",
        help="Daemon controller to use.",
    )


def pytest_configure(config):
    """Validate command line args."""
    logging.basicConfig(level=logging.INFO, format="[%(levelname)s] %(message)s")

    if (
        config.getoption("--daemon-controller") == "standalone"
        and config.getoption("--bin-dir") is None
    ):
        pytest.exit(
            "ERROR: The `--build-root` argument is required when 'standalone' controller is used.",
            returncode=1,
        )


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


def pytest_collection_modifyitems(config, items):
    if (
        config.getoption("--daemon-controller") == "standalone"
        and config.getoption("--driver") == "qemu"
    ):
        ## Check if installed qemu supports uid/gid mapping
        qemu = shutil.which("qemu-system-x86_64")
        if qemu:
            out = subprocess.check_output([qemu, "--help"], text=True)
            if all(s in out for s in ["uid_map", "gid_map"]):
                # Supports uid/gid mapping
                return
    else:
        # Non-standalone deployments use the Multipass-shipped QEMU.
        return

    skip_marker = pytest.mark.skip(
        reason=f"Skipped -- QEMU in environment {qemu} does not support UID/GID mapping."
    )
    for item in items:
        mount = getattr(item, "callspec", None)
        if mount and mount.params.get("mount_type") == "native":
            item.add_marker(skip_marker)


@pytest.fixture(autouse=True, scope="session")
def store_config(request):
    """Store the given command line args in a global variable so
    they would be accessible to all functions in the module."""
    config.bin_dir = request.config.getoption("--bin-dir")
    config.data_root = request.config.getoption("--data-root")
    config.print_daemon_output = request.config.getoption("--print-daemon-output")
    config.print_cli_output = request.config.getoption("--print-cli-output")
    config.remove_all_instances = request.config.getoption("--remove-all-instances")
    config.driver = request.config.getoption("--driver")
    config.daemon_controller = request.config.getoption("--daemon-controller")

    # If user gave --data-root, use it
    if not config.data_root:
        if config.daemon_controller == "standalone":
            # Otherwise, create a temp dir for the whole session
            tmpdir = TempDirectory()

            def cleanup():
                """Since the daemon owns the data_root on startup
                a non-root user cannot remove it. Therefore, try
                privilege escalation if nuking as normal fails."""
                try:
                    shutil.rmtree(str(tmpdir))
                    print(f"\n🧹 ✅ Cleaned up {tmpdir} normally.")
                except PermissionError:
                    print(
                        f"\n🧹 ⚠️ Permission denied, escalating to sudo rm -rf {tmpdir}"
                    )
                    subprocess.run(["sudo", "rm", "-rf", str(tmpdir)], check=True)

            # Register finalizer to cleanup on exit
            request.addfinalizer(cleanup)
            config.data_root = tmpdir.name
        elif config.daemon_controller == "snapd":
            config.data_root = SNAP_MULTIPASSD_STORAGE
            config.bin_dir = SNAP_BIN_DIR


@pytest.fixture(autouse=True, scope="session")
def ensure_sudo_auth():
    """Ensure sudo is authenticated before running tests"""
    try:
        # Test if sudo is already authenticated
        result = subprocess.run(
            [*get_sudo_tool(), "-n", "true"],
            capture_output=True,
            timeout=1,
            check=False,
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
        subprocess.run([*get_sudo_tool(), "-v"], check=True)
    except subprocess.TimeoutExpired:
        pytest.skip("Cannot authenticate sudo non-interactively")


def set_driver(controller):
    if multipass("get", "local.driver") != config.driver:
        assert multipass("set", f"local.driver={config.driver}")
        controller.wait_for_restart()


def make_daemon_controller(kind):
    if kind == "standalone":
        return StandaloneMultipassdController(config.data_root)
    if kind == "snapd":
        return SnapdMultipassdController()
    if kind == "none":
        return None

    raise NotImplementedError(f"No such daemon controller is known: `{kind}`")


@contextmanager
def multipassd_impl():
    if config.daemon_controller == "none":
        sys.stdout.write("Skipping launching the daemon.")
        yield None
        return

    with BackgroundEventLoop() as loop:
        governor = MultipassdGovernor(
            make_daemon_controller(config.daemon_controller),
            loop,
            config.print_daemon_output,
        )
        wait_for_future(loop.run(governor.stop()))
        if config.remove_all_instances:
            run_as_subprocess(nuke_all_instances, config.data_root, privileged=True)
        wait_for_future(loop.run(governor.start()))
        set_driver(governor)
        yield governor
        wait_for_future(loop.run(governor.stop()))


@pytest.fixture(scope="function")
def multipassd(store_config):
    with multipassd_impl() as daemon:
        yield daemon


@pytest.fixture(scope="class")
def multipassd_class_scoped(store_config, request):
    with multipassd_impl() as daemon:
        request.cls.multipassd = daemon
        yield daemon


@pytest.fixture
def windows_privileged_mounts(multipassd):
    if sys.platform == "win32":
        # Check if privileged mounts are already enabled
        if "true" in multipass("get", "local.privileged-mounts"):
            return
        assert multipass("set", "local.privileged-mounts=1")


@pytest.fixture(scope="function")
def instance(request):
    """Launch a VM and ensure cleanup."""

    with launch(request.param if hasattr(request, "param") else None) as inst:  # pylint: disable=R1732
        yield inst
