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

"""Unit tests for `multipass info` helpers."""

import json
from unittest.mock import patch

import pytest

from cli.multipass.cmd_output import Output
from cli.multipass.helpers import info, mounts, state

NO_RUNTIME_INFO = "--no-runtime-information"


@pytest.fixture
def fake_multipass():
    payload = {"info": {"foo": {"state": "Running", "mounts": {}}}}
    with patch(
        "cli.multipass.helpers.multipass",
        return_value=Output(json.dumps(payload), 0),
    ) as mock:
        yield mock


def test_state_skips_runtime_info(fake_multipass):
    # Runtime info needs SSH, which may not be up yet right after start/restart
    assert state("foo") == "Running"
    assert NO_RUNTIME_INFO in fake_multipass.call_args.args


@pytest.mark.parametrize("helper", [info, mounts])
def test_other_helpers_keep_runtime_info(fake_multipass, helper):
    helper("foo")
    assert NO_RUNTIME_INFO not in fake_multipass.call_args.args
