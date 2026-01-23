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

"""Multipass command line tests for the clone feature."""

import pytest

from cli.utilities import is_valid_ipv4_addr
from cli.multipass import (
    multipass,
    validate_info_output,
    take_snapshot,
    snapshot_count,
    state,
    random_vm_name,
    get_default_interface_name,
    get_mac_addr_of,
    get_cloudinit_instance_id,
)


@pytest.mark.clone
@pytest.mark.usefixtures("multipassd")
class TestClone:
    """Clone tests."""

    def test_clone_nonexistent_instance(self):
        name = random_vm_name()
        assert not multipass("clone", f"{name}")

    def test_clone_running_instance(self, instance):
        with multipass("clone", f"{instance}") as output:
            assert not output
            assert "Multipass can only clone stopped instances." in output

    def test_clone_to_self(self, instance):
        assert multipass("stop", f"{instance}")

        with multipass("clone", f"{instance}", "--name", f"{instance}") as output:
            assert not output
            assert "already exist" in output

    def test_clone_instance_with_snapshot(self, instance):
        take_snapshot(instance, "snapshot1")
        with multipass("clone", f"{instance}") as output:
            assert output
            assert f"Cloned from {instance} to {instance}-clone1" in output

        # Verify that the clone does not have any snapshots
        assert snapshot_count(f"{instance}-clone1") == 0
        assert multipass("delete", f"{instance}-clone1", "--purge")

    def test_clone_verify_clone_has_different_properties(self, instance):
        """ip, mac, hostname, etc."""
        assert multipass("stop", instance)
        assert state(instance) == "Stopped"

        expected_clone_name = f"{instance}-clone1"

        with multipass("clone", f"{instance}") as output:
            assert output
            assert f"Cloned from {instance} to {expected_clone_name}" in output

        assert state(expected_clone_name) == "Stopped"

        assert multipass("start", instance)
        assert multipass("start", expected_clone_name)

        validate_info_output(
            expected_clone_name,
            {
                "cpu_count": "2",
                "snapshot_count": "0",
                "state": "Running",
                "mounts": {},
                "image_release": "24.04 LTS",
                "ipv4": is_valid_ipv4_addr,
            },
        )

        assert get_cloudinit_instance_id(instance) != get_cloudinit_instance_id(
            expected_clone_name
        )

        i_if_name = get_default_interface_name(instance)
        i_clone_if_name = get_default_interface_name(expected_clone_name)

        assert get_mac_addr_of(instance, i_if_name) != get_mac_addr_of(
            expected_clone_name, i_clone_if_name
        )
        assert multipass("delete", f"{instance}-clone1", "--purge")
