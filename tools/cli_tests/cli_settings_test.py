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

from cli_tests.multipass import multipass, default_driver_name


@pytest.mark.settings
class TestSettings:

    def test_get_all_keys(self):
        expected_keys = [
            "client.primary-name",
            "local.bridged-network",
            "local.driver",
            "local.image.mirror",
            "local.passphrase",
            "local.privileged-mounts",
        ]
        with multipass("get", "--keys") as keys:
            assert keys
            keys_split = keys.content.split()
            assert keys_split == expected_keys

    def test_get_disabled_primary_name(self):
        assert multipass("set", 'client.primary-name=""')
        assert multipass("get", "client.primary-name") == "<empty>"

    def test_set_primary_name(self):
        assert multipass("set", "client.primary-name=foo")
        assert multipass("get", "client.primary-name") == "foo"

    def test_set_driver_name_to_default(self, multipassd):
        assert multipass("set", f"local.driver={default_driver_name()}")
        # Daemon will autorestart here. We should wait until it's back.
        multipassd.wait_for_restart()
        assert multipass("get", "local.driver") == default_driver_name()
