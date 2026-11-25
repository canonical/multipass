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

"""Unit tests for instance cleanup in launch() and retry exhaustion in multipass()."""

import importlib
from unittest.mock import MagicMock

import pexpect
import pytest

from cli.config import cfg
from cli.multipass import multipass_cmd
from cli.multipass.cmd_output import Output

# `cli.multipass.launch` is shadowed by the re-exported launch() function.
launch_module = importlib.import_module("cli.multipass.launch")
helpers_module = importlib.import_module("cli.multipass.helpers")

VM_NAME = "test-vm"


class FakeMultipass:
    """Records CLI calls and answers `launch` and `delete` with canned results."""

    def __init__(self, launch_result=None, delete_result=None):
        self.calls = []
        self.launch_result = Output("Launched", 0) if launch_result is None else launch_result
        self.delete_result = Output("Deleted", 0) if delete_result is None else delete_result

    def __call__(self, *args, **kwargs):
        self.calls.append(args)
        result = {"launch": self.launch_result, "delete": self.delete_result}[args[0]]
        if isinstance(result, BaseException):
            raise result
        return result

    @property
    def deleted(self):
        return [args for args in self.calls if args[0] == "delete"]


@pytest.fixture
def fake_env(monkeypatch):
    for attr, value in (("cpus", 2), ("memory", "1G"), ("disk", "5G"), ("image", "noble")):
        monkeypatch.setattr(cfg.vm, attr, value, raising=False)

    # First call: launch() asserts the name is free. Second: cleanup checks it exists.
    vm_exists = MagicMock(side_effect=[False, True])
    state = MagicMock(return_value="Running")
    for module in (launch_module, helpers_module):
        monkeypatch.setattr(module, "vm_exists", vm_exists)
    monkeypatch.setattr(launch_module, "mounts", MagicMock(return_value={}))
    monkeypatch.setattr(launch_module, "state", state)
    monkeypatch.setattr(launch_module, "random_vm_name", lambda: VM_NAME)

    def install(fake):
        for module in (launch_module, helpers_module):
            monkeypatch.setattr(module, "multipass", fake)
        return fake

    return install, vm_exists, state


class TestLaunchCleanup:
    """launch() must not leak instances, whatever fails."""

    def test_deletes_instance_when_post_launch_check_fails(self, fake_env):
        install, _, state = fake_env
        fake = install(FakeMultipass())
        state.return_value = "Starting"

        with pytest.raises(AssertionError):
            with launch_module.launch():
                pytest.fail("Body must not run")

        assert fake.deleted == [("delete", VM_NAME, "--force")]

    def test_deletes_instance_when_launch_times_out(self, fake_env):
        install, _, _ = fake_env
        fake = install(FakeMultipass(launch_result=pexpect.exceptions.TIMEOUT("t")))

        with pytest.raises(pexpect.exceptions.TIMEOUT):
            with launch_module.launch():
                pytest.fail("Body must not run")

        assert fake.deleted == [("delete", VM_NAME, "--force")]

    def test_failed_cleanup_does_not_mask_test_failure(self, fake_env):
        install, _, _ = fake_env
        fake = install(FakeMultipass(delete_result=pexpect.exceptions.TIMEOUT("t")))

        with pytest.raises(RuntimeError, match="boom"):
            with launch_module.launch():
                raise RuntimeError("boom")

        assert fake.deleted == [("delete", VM_NAME, "--force")]

    def test_reports_failed_delete_after_successful_test(self, fake_env):
        install, _, _ = fake_env
        install(FakeMultipass(delete_result=Output("cannot delete", 1)))

        with pytest.raises(AssertionError, match="Failed to delete VM"):
            with launch_module.launch():
                pass

    def test_no_autodelete_removes_instance_left_behind(self, fake_env):
        install, _, _ = fake_env
        fake = install(FakeMultipass())

        with launch_module.launch({"autodelete": False}):
            pass

        assert fake.deleted == [("delete", VM_NAME, "--force")]

    def test_no_autodelete_keeps_hands_off_deleted_instance(self, fake_env):
        install, vm_exists, _ = fake_env
        fake = install(FakeMultipass())
        vm_exists.side_effect = [False, False]

        with launch_module.launch({"autodelete": False}):
            pass

        assert fake.deleted == []


def test_ensure_deleted_on_exit_deletes_leftover_and_propagates_error(fake_env):
    install, vm_exists, _ = fake_env
    fake = install(FakeMultipass())
    vm_exists.side_effect = [True]

    with pytest.raises(RuntimeError, match="boom"):
        with helpers_module.ensure_deleted_on_exit("clone-vm"):
            raise RuntimeError("boom")

    assert fake.deleted == [("delete", "clone-vm", "--force")]


def test_multipass_raises_timeout_when_all_retries_time_out(monkeypatch):
    monkeypatch.setattr(cfg, "active_governor", None, raising=False)
    monkeypatch.setattr(cfg, "print_cli_output", False, raising=False)
    monkeypatch.setattr(multipass_cmd, "get_multipass_path", lambda: "multipass")
    monkeypatch.setattr(multipass_cmd, "get_multipass_env", dict)
    spawn = MagicMock(side_effect=pexpect.exceptions.TIMEOUT("t"))
    monkeypatch.setattr(multipass_cmd, "spawn_multipass", spawn)
    monkeypatch.setattr("cli.utilities.functional.time.sleep", lambda _: None)

    with pytest.raises(pexpect.exceptions.TIMEOUT, match="timed out on all 2 attempts"):
        multipass_cmd.multipass("list", retry=1)

    assert spawn.call_count == 2
