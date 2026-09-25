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

"""Multipass command line tests for availability zones."""

import pytest

from cli.multipass import (
    exec,
    info,
    launch,
    multipass,
    skip_if_feature_not_supported,
    state,
)


@pytest.mark.az
@pytest.mark.usefixtures("multipassd")
class TestAvailabilityZones:
    """CLI availability-zone tests."""

    def test_enable_disable_zone_states(self):
        """Check that enabling/disabling zones updates the instance states."""

        skip_if_feature_not_supported("az")

        with (
            launch({"zone": "zone1"}) as instance1,
            launch({"zone": "zone1"}) as instance2,
        ):
            multipass("stop", f"{instance2}", "--force")
            assert state(instance1) == "Running"
            assert state(instance2) == "Stopped"

            assert multipass("disable-zones", "zone1", "--force")
            assert state(instance1) == "Unavailable"
            assert state(instance2) == "Unavailable"

            assert multipass("enable-zones", "zone1")
            assert state(instance1) == "Starting"
            assert state(instance2) == "Stopped"

    def test_instances_in_different_zones_can_communicate(self):
        """Check that instances can communicate across zone subnets."""

        skip_if_feature_not_supported("az")

        with (
            launch({"zone": "zone1"}) as zone1_instance,
            launch({"zone": "zone2"}) as zone2_instance,
        ):
            zone1_ip = info(zone1_instance)[zone1_instance]["ipv4"][0]
            zone2_ip = info(zone2_instance)[zone2_instance]["ipv4"][0]

            assert exec(zone1_instance, "ping", "-c", "3", "-W", "2", zone2_ip)
            assert exec(zone2_instance, "ping", "-c", "3", "-W", "2", zone1_ip)
