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

"""Client settings should survive an upgrade."""

import pytest

from cli.multipass import multipass

VM = "upg-settings"
SETTING = "client.primary-name"
VALUE = "upg-settings"


@pytest.mark.seed
@pytest.mark.scenario(VM)
def test_settings_seed(scenario):
    assert multipass("set", f"{SETTING}={VALUE}")
    assert multipass("get", SETTING) == VALUE

    scenario.record.update({"setting": SETTING, "value": VALUE})


@pytest.mark.verify
@pytest.mark.scenario(VM)
def test_settings_verify(scenario, request):
    request.addfinalizer(lambda: multipass("set", f"{SETTING}="))

    recorded = scenario.record
    assert multipass("get", recorded["setting"]) == recorded["value"]
