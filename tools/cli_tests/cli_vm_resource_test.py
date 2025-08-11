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

"""Multipass command line e2e tests"""

import pytest

from cli_tests.utilities import is_within_tolerance

from cli_tests.multipass import (
    get_ram_size,
    get_core_count,
    get_disk_size,
    validate_list_output,
    multipass,
    random_vm_name
)


@pytest.mark.resource
@pytest.mark.usefixtures("multipassd")
class TestVmResource:

    def test_modify_vm_while_running(self):
        """Launch an Ubuntu 22.04 VM with 2 CPUs 1GiB RAM and 6G disk.
        Then, try to shell into it and execute some basic commands."""
        name = random_vm_name()

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
            "jammy",
            retry=3,
        )

        validate_list_output(name, {"state": "Running"})

        assert is_within_tolerance(get_ram_size(name), 1024)
        assert is_within_tolerance(get_disk_size(name), 6144)
        assert get_core_count(name) == 2

        assert not multipass("set", f"local.{name}.memory=2G")
        assert not multipass("set", f"local.{name}.disk=10G")
        assert not multipass("set", f"local.{name}.cpus=3")

        assert is_within_tolerance(get_ram_size(name), 1024)
        assert is_within_tolerance(get_disk_size(name), 6144)
        assert get_core_count(name) == 2

        # Remove the instance.
        assert multipass("delete", f"{name}")

    def test_modify_vm(self):
        """Launch an Ubuntu 22.04 VM with 2 CPUs 1GiB RAM and 6G disk.
        Then, try to shell into it and execute some basic commands."""
        name = random_vm_name()

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
            "jammy",
            retry=3,
        )

        validate_list_output(name, {"state": "Running"})

        assert is_within_tolerance(get_ram_size(name), 1024)
        assert is_within_tolerance(get_disk_size(name), 6144)
        assert get_core_count(name) == 2

        assert multipass("stop", name)
        assert multipass("set", f"local.{name}.memory=2G")
        assert multipass("set", f"local.{name}.disk=10G")
        assert multipass("set", f"local.{name}.cpus=3")

        assert multipass("start", name)

        assert is_within_tolerance(get_ram_size(name), 2048)
        assert is_within_tolerance(get_disk_size(name), 10240)
        assert get_core_count(name) == 3

        # Remove the instance.
        assert multipass("delete", f"{name}")
