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

"""Multipass command line tests for the VM lifecycle."""

import pytest

from cli.multipass import multipass, state, launch, random_vm_name, get_boot_id


@pytest.mark.lifecycle
@pytest.mark.usefixtures("multipassd")
class TestVmLifecycle:
    """Virtual machine lifecycle tests."""

    def test_lifecycle_ops_on_nonexistent(self):
        name = random_vm_name()
        ops = [
            "start",
            "suspend",
            "stop",
            "delete",
            "recover",
            "restart",
        ]

        for op in ops:
            with multipass(op, f"{name}", disable_hooks=True) as output:
                assert not output
                assert "does not exist" in output

    def test_stop(self, instance):
        assert state(f"{instance}") == "Running"
        assert multipass("stop", f"{instance}")
        assert state(f"{instance}") == "Stopped"

    def test_stop_start(self, instance):
        assert state(f"{instance}") == "Running"
        boot_id_before = get_boot_id(instance)
        assert multipass("stop", f"{instance}")
        assert state(f"{instance}") == "Stopped"
        assert multipass("start", f"{instance}")
        assert state(f"{instance}") == "Running"
        assert boot_id_before != get_boot_id(instance)

    def test_suspend_resume(self, instance):
        assert state(f"{instance}") == "Running"
        boot_id_before = get_boot_id(instance)
        assert multipass("suspend", f"{instance}")
        assert state(f"{instance}") == "Suspended"
        assert multipass("start", f"{instance}")
        assert state(f"{instance}") == "Running"
        boot_id_after = get_boot_id(instance)
        # Must have the same boot id as before
        assert boot_id_before == boot_id_after

    @pytest.mark.parametrize(
        "instance",
        [
            {"autopurge": False},
        ],
        indirect=True,
    )
    def test_launch_delete_recover_purge(self, instance):
        assert state(f"{instance}") == "Running"
        assert multipass("delete", f"{instance}")
        assert state(f"{instance}") == "Deleted"

        with multipass("start", f"{instance}") as output:
            assert not output
            assert f"Instance '{instance}' is deleted." in output

        assert multipass("recover", f"{instance}")
        assert state(f"{instance}") == "Stopped"

        assert multipass("start", f"{instance}")

        assert multipass("delete", f"{instance}")
        assert state(f"{instance}") == "Deleted"

        assert multipass("delete", f"{instance}", "--purge")
        with multipass("info", f"{instance}") as output:
            assert not output
            assert "does not exist" in output

    def test_delete_purge(self):
        with (
            launch({"autopurge": False}) as name1,
            launch({"autopurge": False}) as name2,
        ):
            assert multipass("delete", f"{name1}")
            assert state(f"{name1}") == "Deleted"

            assert multipass("delete", f"{name2}")
            assert state(f"{name2}") == "Deleted"

            assert multipass("purge")

            for instance in [name1, name2]:
                with multipass("info", f"{instance}") as output:
                    assert not output
                    assert "does not exist" in output

    def test_lifecycle_multiple(self):
        with (
            launch({"autopurge": False}) as name1,
            launch({"autopurge": False}) as name2,
        ):
            assert multipass("stop", name1, name2)
            assert state(name1) == "Stopped" and state(name2) == "Stopped"

            assert multipass("start", name1, name2)
            assert state(name1) == "Running" and state(name2) == "Running"

            assert multipass("suspend", name1, name2)
            assert state(name1) == "Suspended" and state(name2) == "Suspended"

            assert multipass("start", name1, name2)
            assert state(name1) == "Running" and state(name2) == "Running"

            vm1_boot_id_before_restart = get_boot_id(name1)
            vm2_boot_id_before_restart = get_boot_id(name2)

            assert multipass("restart", name1, name2)
            assert state(name1) == "Running" and state(name2) == "Running"

            vm1_boot_id_after_restart = get_boot_id(name1)
            vm2_boot_id_after_restart = get_boot_id(name2)
            assert vm1_boot_id_before_restart != vm1_boot_id_after_restart
            assert vm2_boot_id_before_restart != vm2_boot_id_after_restart

            assert multipass("delete", name1, name2)
            assert state(name1) == "Deleted" and state(name2) == "Deleted"

            assert multipass("recover", name1, name2)
            assert state(name1) == "Stopped" and state(name2) == "Stopped"

            assert multipass("delete", name1, name2)
            assert state(name1) == "Deleted" and state(name2) == "Deleted"

            assert multipass("purge")

            with multipass("info", name1, name2) as output:
                assert not output
                assert f'instance "{name1}" does not exist' in output
                assert f'instance "{name2}" does not exist' in output
