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

from cli_tests.utilities import uuid4_str, is_valid_ipv4_addr
from cli_tests.multipass import multipass, validate_info_output, take_snapshot


@pytest.mark.clone
class TestClone:
    """Clone tests."""

    def test_clone_nonexistent_instance(self):
        name = uuid4_str("instance")
        assert not multipass("clone", f"{name}")

    def test_clone_running_instance(self):
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

        with multipass("clone", f"{name}") as output:
            assert not output
            assert "Multipass can only clone stopped instances." in output

    def test_clone_to_self(self):
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
        assert multipass("stop", f"{name}")

        with multipass("clone", f"{name}", "--name", f"{name}") as output:
            assert not output
            assert "already exist" in output

    def test_clone_instance_with_snapshot(self):
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
        take_snapshot(name, "snapshot1")
        with multipass("clone", f"{name}") as output:
            assert output
            assert f"Cloned from {name} to {name}.clone1" in output

        # Verify that the clone does not have any snapshots
        validate_info_output(f"{name}-clone1", {"snapshot_count": "0"})

    def test_clone_verify_clone_has_different_properties(self):
        """ip, mac, hostname, etc."""
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

        assert multipass("stop", f"{name}")
        validate_info_output(name, {"state": "Stopped"})

        with multipass("clone", f"{name}") as output:
            assert output
            assert f"Cloned from {name} to {name}-clone1" in output

        validate_info_output(f"{name}-clone1", {"state": "Stopped"})
        assert multipass("start", f"{name}")
        assert multipass("start", f"{name}-clone1")

        validate_info_output(
            f"{name}-clone1",
            {
                "cpu_count": "2",
                "snapshot_count": "0",
                "state": "Running",
                "mounts": {},
                "image_release": "24.04 LTS",
                "ipv4": is_valid_ipv4_addr,
            },
        )

        src_cloud_init_instance_id = multipass(
            "exec", f"{name}", "--", "cat /var/lib/cloud/data/instance-id"
        )
        clone_cloud_init_instance_id = multipass(
            "exec", f"{name}-clone1", "--", "cat /var/lib/cloud/data/instance-id"
        )

        assert src_cloud_init_instance_id
        assert clone_cloud_init_instance_id

        assert (
            src_cloud_init_instance_id.content != clone_cloud_init_instance_id.content
        )

        assert f"{name}-clone1" in multipass("exec", f"{name}-clone1", "--", "hostname")

        # Verify that clone's primary interface has a different MAC address than
        # the src.
        with multipass(
            "exec",
            f"{name}",
            "--",
            "bash",
            "-c",
            "\"ip route get 1 | awk '{print $5}' | xargs -I{} cat /sys/class/net/{}/address\"",
        ) as src_mac, multipass(
            "exec",
            f"{name}-clone1",
            "--",
            "bash",
            "-c",
            "\"ip route get 1 | awk '{print $5}' | xargs -I{} cat /sys/class/net/{}/address\"",
        ) as clone_mac:
            assert src_mac and clone_mac
            assert src_mac.content != clone_mac.content
