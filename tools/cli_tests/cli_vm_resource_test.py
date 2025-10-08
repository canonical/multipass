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

"""Multipass VM resource tests"""

import pytest

from cli_tests.utilities import is_within_tolerance

from cli_tests.multipass import (
    get_ram_size,
    get_core_count,
    get_disk_size,
    multipass,
)


@pytest.mark.resource
@pytest.mark.usefixtures("multipassd")
class TestVmResource:
    """Virtual machine resource CLI tests."""

    @pytest.mark.parametrize(
        "instance",
        [
            {"cpus": 2, "memory": "1G", "disk": "6G", "image": "jammy"},
        ],
        indirect=True,
    )
    def test_modify_vm_while_running(self, instance):
        """Launch a new instance and try changing its resources while the
        instance is running."""

        assert is_within_tolerance(get_ram_size(instance), 1024)
        assert is_within_tolerance(get_disk_size(instance), 6144)
        assert get_core_count(instance) == 2

        assert not multipass("set", f"local.{instance}.memory=2G")
        assert not multipass("set", f"local.{instance}.disk=10G")
        assert not multipass("set", f"local.{instance}.cpus=3")

        assert is_within_tolerance(get_ram_size(instance), 1024)
        assert is_within_tolerance(get_disk_size(instance), 6144)
        assert get_core_count(instance) == 2

    @pytest.mark.parametrize(
        "instance",
        [
            {"cpus": 2, "memory": "1G", "disk": "6G", "image": "jammy"},
        ],
        indirect=True,
    )
    def test_modify_vm(self, instance):
        """Launch a new instance and try changing its resources while the
        instance is stopped."""

        assert is_within_tolerance(get_ram_size(instance), 1024)
        assert is_within_tolerance(get_disk_size(instance), 6144)
        assert get_core_count(instance) == 2

        assert multipass("stop", instance)
        assert multipass("set", f"local.{instance}.memory=2G")
        assert multipass("set", f"local.{instance}.disk=10G")
        assert multipass("set", f"local.{instance}.cpus=3")

        assert multipass("start", instance)

        assert is_within_tolerance(get_ram_size(instance), 2048)
        assert is_within_tolerance(get_disk_size(instance), 10240)
        assert get_core_count(instance) == 3
