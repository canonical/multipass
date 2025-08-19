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
from contextlib import contextmanager, ExitStack
from functools import partial

import pytest
from pytest import Session

from cli_tests.utilities import (
    wait_for_future,
    get_sudo_tool,
    run_in_new_interpreter,
    BackgroundEventLoop,
    TempDirectory,
)

from cli_tests.multipass import (
    Output,
    multipass,
    default_driver_name,
    default_daemon_controller,
    launch,
    nuke_all_instances,
    SNAP_MULTIPASSD_STORAGE,
    LAUNCHD_MULTIPASSD_STORAGE,
    SNAP_BIN_DIR,
    LAUNCHD_MULTIPASS_BIN_DIR,
)

from cli_tests.config import config
from cli_tests.controller import (
    MultipassdGovernor,
    SnapdMultipassdController,
    StandaloneMultipassdController,
    LaunchdMultipassdController,
    ControllerPrerequisiteError,
)


def pytest_addoption(parser):
    """Populate command line args for the CLI tests."""

    def kv_option_type(arg):
        name, n = arg.split("=", 1)
        return (name, int(n))

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
        "--print-all-output",
        action="store_true",
        help="Shorthand for --print-daemon-output and --print-cli-output.",
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
        default=default_daemon_controller(),
        help="Daemon controller to use.",
        choices=["standalone", "snapd", "launchd", "none"],
    )

    parser.addoption(
        "--vm-cpus",
        default="2",
        help="Default CPU count to use for launching test VMs.",
    )

    parser.addoption(
        "--vm-memory",
        default="1G",
        help="Default memory size to use for launching test VMs.",
    )

    parser.addoption(
        "--vm-disk",
        default="6G",
        help="Default disk size to use for launching test VMs.",
    )

    parser.addoption(
        "--vm-image",
        default="noble",
        help="Default image to use for launching test VMs.",
    )

    parser.addoption(
        "--cmd-retries",
        action="extend",
        nargs="+",
        dest="cmd_retries",
        type=kv_option_type,
        metavar="CMD=N[ CMD=N...]",
        default=[("launch", 3)],
        help="Per-command retry override; may be given multiple times. "
        "Example: --retries launch=3 delete=1",
    )

    parser.addoption(
        "--cmd-timeouts",
        action="extend",
        nargs="+",
        dest="cmd_timeouts",
        type=kv_option_type,
        metavar="CMD=N[,CMD=N...]",
        default=[
            ("launch", 600),
            ("stop", 180),
            ("mount", 180),
            ("restart", 120),
            ("delete", 90),
            ("exec", 90),
            ("start", 90),
            ("umount", 45),
        ],
        help="Per-command timeout override; may be given multiple times. "
        "Example: --timeouts launch=180 start=60",
    )


def pytest_configure(config):
    """Validate command line args."""
    logging.basicConfig(level=logging.INFO, format="[%(levelname)s] %(message)s")

    if (
        config.getoption("--daemon-controller") == "standalone"
        and config.getoption("--bin-dir") is None
    ):
        pytest.exit(
            "ERROR: The `--bin-dir` argument is required when 'standalone' controller is used.",
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
    def maybe_skip_mount_test(item):
        callspec = getattr(item, "callspec", None)
        if not item.get_closest_marker("mount"):
            return
        if not callspec.params.get("mount_type") == "native":
            return

        if sys.platform == "win32":
            item.add_marker(
                pytest.mark.skip(
                    "Skipped -- Testing native mounts in Windows is not supported yet."
                )
            )
            return

        if (
            config.getoption("--daemon-controller") == "standalone"
            and config.getoption("--driver") == "qemu"
        ):
            qemu = shutil.which("qemu-system-x86_64")
            if qemu:
                out = subprocess.check_output([qemu, "--help"], text=True)
                if all(s in out for s in ["uid_map", "gid_map"]):
                    # Supports uid/gid mapping
                    return
            item.add_marker(
                pytest.mark.skip(
                    f"Skipped -- QEMU in environment ({qemu}) does not support UID/GID mapping."
                )
            )
            return

    def maybe_skip_clone_test(item):
        if not item.get_closest_marker("clone"):
            return

        if config.getoption("--driver") == "lxd":
            item.add_marker(
                pytest.mark.skip(
                    "Skipped -- LXD driver does not support the `clone` feature."
                )
            )

    def maybe_skip_snapshot_test(item):
        if not item.get_closest_marker("snapshot"):
            return
        if config.getoption("--driver") == "lxd":
            item.add_marker(
                pytest.mark.skip(
                    "Skipped -- LXD driver does not support the `clone` feature."
                )
            )

    for item in items:
        maybe_skip_mount_test(item)
        maybe_skip_clone_test(item)
        maybe_skip_snapshot_test(item)


@pytest.fixture(autouse=True, scope="session")
def store_config(request):
    """Store the given command line args in a global variable so
    they would be accessible to all functions in the module."""
    config.bin_dir = request.config.getoption("--bin-dir")
    config.data_root = request.config.getoption("--data-root")
    config.print_daemon_output = request.config.getoption("--print-daemon-output")
    config.print_cli_output = request.config.getoption("--print-cli-output")

    if request.config.getoption("--print-all-output"):
        config.print_daemon_output = True
        config.print_cli_output = True

    config.remove_all_instances = request.config.getoption("--remove-all-instances")
    config.driver = request.config.getoption("--driver")
    config.daemon_controller = request.config.getoption("--daemon-controller")

    config.vm.cpus = request.config.getoption("--vm-cpus")
    config.vm.memory = request.config.getoption("--vm-memory")
    config.vm.disk = request.config.getoption("--vm-disk")
    config.vm.image = request.config.getoption("--vm-image")

    for name, value in request.config.getoption("cmd_retries"):
        setattr(config.retries, name, value)

    for name, value in request.config.getoption("cmd_timeouts"):
        setattr(config.timeouts, name, value)

    # If user gave --data-root, use it
    if not config.data_root:
        if config.daemon_controller == "standalone":
            # Otherwise, create a temp dir for the whole session
            with TempDirectory(delete=False) as tmpdir:

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
                config.data_root = str(tmpdir)
        elif config.daemon_controller == "snapd":
            config.data_root = SNAP_MULTIPASSD_STORAGE
            config.bin_dir = SNAP_BIN_DIR
        elif config.daemon_controller == "launchd":
            config.data_root = LAUNCHD_MULTIPASSD_STORAGE
            config.bin_dir = LAUNCHD_MULTIPASS_BIN_DIR

    logging.info(f"store_config :: final config {config}")


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
    # Return a partial so the controller can be instantiated on demand.
    controllers = {
        "standalone": partial(StandaloneMultipassdController, config.data_root),
        "snapd": partial(SnapdMultipassdController),
        "launchd": partial(LaunchdMultipassdController),
        "none": lambda: None,
    }

    if kind in controllers:
        return controllers[kind]

    raise NotImplementedError(f"No such daemon controller is known: `{kind}`")


@pytest.fixture(scope="session", autouse=True)
def environment_setup(store_config):
    cntrl = make_daemon_controller(config.daemon_controller)
    controller_class = cntrl.func

    def has_static_method(cls, name):
        for base in cls.__mro__:
            if name in base.__dict__:
                return isinstance(base.__dict__[name], staticmethod)
        return False

    logging.debug("environment_setup :: setup")
    if has_static_method(controller_class, "setup_environment"):
        controller_class.setup_environment()

    yield

    logging.debug("environment_setup :: teardown")
    if has_static_method(controller_class, "teardown_environment"):
        controller_class.teardown_environment()


@contextmanager
def multipassd_impl():
    if config.daemon_controller == "none":
        sys.stdout.write("Skipping launching the daemon.")
        yield None
        return

    try:
        controller = make_daemon_controller(config.daemon_controller)()
    except ControllerPrerequisiteError as exc:
        Session.shouldfail = str(exc)
        raise

    with ExitStack() as stack:
        loop = stack.enter_context(BackgroundEventLoop())
        governor = MultipassdGovernor(
            controller,
            loop,
            config.print_daemon_output,
        )

        # Ensure that the governor.stop() is called on context exit.
        stack.callback(lambda: wait_for_future(loop.run(governor.stop_async())))

        # Stop the governor if already running (for cleanup)
        wait_for_future(loop.run(governor.stop_async()))

        if config.remove_all_instances:
            run_in_new_interpreter(
                nuke_all_instances, config.data_root, privileged=True
            )

        # Start the governor
        wait_for_future(loop.run(governor.start_async()))

        # Ensure the right driver is set
        set_driver(governor)

        yield governor


@pytest.fixture(scope="function")
def multipassd(store_config):
    with multipassd_impl() as daemon:
        yield daemon


@pytest.fixture
def feat_snapshot(store_config):
    yield config.driver != "lxd"


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
