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

"""Multipass command line tests for the `multipass launch`."""

import pytest

from cli_tests.utilities import is_valid_ipv4_addr
from cli_tests.multipass import (
    multipass,
    validate_list_output,
    validate_info_output,
    image_name_to_version,
    random_vm_name,
    snapshot_count,
    info,
    state,
    vm_exists,
    test_requires_feature
)


@pytest.mark.launch
@pytest.mark.usefixtures("multipassd")
class TestLaunch:
    """CLI VM launch tests."""

    @pytest.mark.parametrize(
        "instance",
        [
            {"image": "noble", "autopurge": False},
            {"image": "jammy", "autopurge": False},
            {"image": "focal", "autopurge": False},
        ],
        indirect=True,
    )
    def test_launch_ubuntu(self, instance, feat_snapshot):
        """Try to launch an Ubuntu 24.04 VM with default specs.
        Then, validate the basics."""

        validate_list_output(
            instance,
            {
                "state": "Running",
                "release": f"Ubuntu {image_name_to_version(instance.image)} LTS",
                "ipv4": is_valid_ipv4_addr,
            },
        )

        validate_info_output(
            instance,
            {
                "cpu_count": "2",
                "state": "Running",
                "mounts": {},
                "image_release": f"{image_name_to_version(instance.image)} LTS",
                "ipv4": is_valid_ipv4_addr,
            },
        )

        if feat_snapshot:
            assert snapshot_count(instance) == 0
        else:
            assert "snapshot_count" not in info(instance)

        # Try to stop the instance
        assert multipass("stop", instance)
        assert state(instance) == "Stopped"

        # Try to start the instance
        assert multipass("start", instance)
        assert state(instance) == "Running"

        # Remove the instance.
        assert multipass("delete", instance)
        assert state(instance) == "Deleted"

        assert multipass("purge")

        assert not vm_exists(instance)

    @pytest.mark.parametrize(
        "instance, param",
        [
            ({"image": "debian", "autopurge": False}, {"expected_release": "Trixie"}),
        ],
        indirect=["instance"],
    )
    def test_launch_debian(self, instance, param, feat_snapshot):
        """Try to launch a Debian VM with default specs.
        Then, validate the basics."""

        test_requires_feature("debian_images")

        validate_list_output(
            instance,
            {
                "state": "Running",
                "release": f"Debian {param["expected_release"]}",
                "ipv4": is_valid_ipv4_addr,
            },
        )

        validate_info_output(
            instance,
            {
                "cpu_count": "2",
                "state": "Running",
                "mounts": {},
                "image_release": param["expected_release"],
                "ipv4": is_valid_ipv4_addr,
            },
        )

        if feat_snapshot:
            assert snapshot_count(instance) == 0
        else:
            assert "snapshot_count" not in info(instance)

        # Try to stop the instance
        assert multipass("stop", instance)
        assert state(instance) == "Stopped"

        # Try to start the instance
        assert multipass("start", instance)
        assert state(instance) == "Running"

        # Remove the instance.
        assert multipass("delete", instance)
        assert state(instance) == "Deleted"

        assert multipass("purge")

        assert not vm_exists(instance)

    @pytest.mark.parametrize(
        "instance, param",
        [
            ({"image": "fedora", "autopurge": False}, {"expected_release": "43"}),
        ],
        indirect=["instance"],
    )
    def test_launch_fedora(self, instance, param, feat_snapshot):
        """Try to launch a Fedora VM with default specs.
        Then, validate the basics."""

        test_requires_feature("fedora_images")

        validate_list_output(
            instance,
            {
                "state": "Running",
                "release": f"Fedora {param["expected_release"]}",
                "ipv4": is_valid_ipv4_addr,
            },
        )

        validate_info_output(
            instance,
            {
                "cpu_count": "2",
                "state": "Running",
                "mounts": {},
                "image_release": param["expected_release"],
                "ipv4": is_valid_ipv4_addr,
            },
        )

        if feat_snapshot:
            assert snapshot_count(instance) == 0
        else:
            assert "snapshot_count" not in info(instance)

        # Try to stop the instance
        assert multipass("stop", instance)
        assert state(instance) == "Stopped"

        # Try to start the instance
        assert multipass("start", instance)
        assert state(instance) == "Running"

        # Remove the instance.
        assert multipass("delete", instance)
        assert state(instance) == "Deleted"

        assert multipass("purge")

        assert not vm_exists(instance)

    def test_launch_invalid_name(self):
        invalid_names = [
            "1nvalid-name",
            "invalid.name",
            "invalid$name",
            "invalid(name)",
            "invalid-name-",
        ]

        for name in invalid_names:
            with multipass(
                "launch",
                "--cpus",
                "2",
                "--memory",
                "1G",
                "--disk",
                "6G",
                "--name",
                name,
            ) as output:
                assert not output
                assert "Invalid instance name supplied" in output
                assert not vm_exists(invalid_names)

    def test_launch_invalid_ram(self):
        name = random_vm_name()
        with multipass(
            "launch",
            "--cpus",
            "2",
            "--memory",
            "1CiG",
            "--disk",
            "6G",
            "--name",
            name,
        ) as output:
            assert not output
            assert "1CiG is not a valid memory size" in output
            assert not vm_exists(name)

    def test_launch_invalid_disk(self):
        name = random_vm_name()
        with multipass(
            "launch",
            "--cpus",
            "2",
            "--memory",
            "1G",
            "--disk",
            "6CiG",
            "--name",
            name,
        ) as output:
            assert not output
            assert "6CiG is not a valid memory size" in output
            assert not vm_exists(name)
