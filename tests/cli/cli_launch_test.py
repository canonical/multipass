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

import os
import platform
import urllib.request
from pathlib import Path

import pytest

from cli.utilities import TempDirectory, is_valid_ipv4_addr
from cli.multipass import (
    launch,
    multipass,
    validate_list_output,
    validate_info_output,
    image_name_to_version,
    random_vm_name,
    snapshot_count,
    info,
    state,
    vm_exists,
    skip_if_feature_not_supported
)


# Lightest official Ubuntu image (minimal cloud image, ~250 MB)
MINIMAL_IMAGE_URL = (
    "https://cloud-images.ubuntu.com/minimal/releases/noble/release/"
    "ubuntu-24.04-minimal-cloudimg-{arch}.img"
)


@pytest.fixture(scope="session")
def local_image():
    """Download a minimal Ubuntu image (once, then cached), and return its
    absolute path.
    """

    arch = "arm64" if platform.machine().lower() in ("arm64", "aarch64") else "amd64"
    with TempDirectory() as folder:
        path = folder / f"ubuntu-minimal-{arch}.img"
        try:
            urllib.request.urlretrieve(MINIMAL_IMAGE_URL.format(arch=arch), path)
        except OSError as exc:
            pytest.skip(f"Could not download the minimal Ubuntu image: {exc}")

        yield str(path)


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
        "instance",
        [
            {"image": "debian", "autopurge": False},
        ],
        indirect=True,
    )
    def test_launch_debian(self, instance, feat_snapshot):
        """Try to launch a Debian VM with default specs.
        Then, validate the basics."""

        skip_if_feature_not_supported("debian_images")

        validate_list_output(
            instance,
            {
                "state": "Running",
                "release": lambda r: r.startswith("Debian "),
                "ipv4": is_valid_ipv4_addr,
            },
        )

        validate_info_output(
            instance,
            {
                "cpu_count": "2",
                "state": "Running",
                "mounts": {},
                "image_release": lambda r: bool(r),
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
        "instance",
        [
            {"image": "fedora", "autopurge": False},
        ],
        indirect=True,
    )
    def test_launch_fedora(self, instance, feat_snapshot):
        """Try to launch a Fedora VM with default specs.
        Then, validate the basics."""

        skip_if_feature_not_supported("fedora_images")

        validate_list_output(
            instance,
            {
                "state": "Running",
                "release": lambda r: r.startswith("Fedora "),
                "ipv4": is_valid_ipv4_addr,
            },
        )

        validate_info_output(
            instance,
            {
                "cpu_count": "2",
                "state": "Running",
                "mounts": {},
                "image_release": lambda r: bool(r),
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
                assert not vm_exists(name)

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

    def test_launch_local_absolute_path(self, local_image):
        """Launching from a plain absolute path must work."""

        with launch({"image": "file://" + local_image}) as vm:
            assert state(vm) == "Running"

    def test_launch_local_relative_path(self, local_image):
        """Launching from a plain relative path must work."""

        relative = os.path.relpath(local_image)
        if not relative.startswith("."):
            relative = os.path.join(".", relative)

        with launch({"image": "file://" + relative}) as vm:
            assert state(vm) == "Running"
