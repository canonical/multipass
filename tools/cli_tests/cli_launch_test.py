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

from cli_tests.utilities import uuid4_str, is_valid_ipv4_addr
from cli_tests.multipass import multipass, validate_list_output, validate_info_output


@pytest.mark.launch
@pytest.mark.usefixtures("multipassd")
class TestLaunch:
    """CLI VM launch tests."""

    def test_launch_noble(self):
        """Try to launch an Ubuntu 24.04 VM with 2 CPUs 1GiB RAM and 6G disk.
        Then, validate the basics."""
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
            "noble",
            retry=3,
        )

        validate_list_output(
            name,
            {
                "state": "Running",
                "release": "Ubuntu 24.04 LTS",
                "ipv4": is_valid_ipv4_addr,
            },
        )

        validate_info_output(
            name,
            {
                "cpu_count": "2",
                "snapshot_count": "0",
                "state": "Running",
                "mounts": {},
                "image_release": "24.04 LTS",
                "ipv4": is_valid_ipv4_addr,
            },
        )

        # Try to stop the instance
        assert multipass("stop", f"{name}")
        validate_info_output(name, {"state": "Stopped"})

        # Try to start the instance
        assert multipass("start", f"{name}")
        validate_info_output(name, {"state": "Running"})

        # Remove the instance.
        assert multipass("delete", f"{name}")
        validate_info_output(name, {"state": "Deleted"})
