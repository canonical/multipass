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

"""Unit tests for the instance name resolution of the `restart` test hooks."""

import json
from unittest.mock import MagicMock, patch

import pytest

from cli.multipass import multipass_cmd
from cli.multipass.cmd_json_output import JsonOutput

LIST_OUTPUT = {
    "list": [
        {"name": "running", "state": "Running"},
        {"name": "stopped", "state": "Stopped"},
        {"name": "deleted", "state": "Deleted"},
    ]
}


@pytest.fixture
def mock_list():
    result = MagicMock()
    result.json.return_value = JsonOutput(json.dumps(LIST_OUTPUT), 0)
    with patch.object(multipass_cmd, "multipass", return_value=result) as mock:
        yield mock


@pytest.mark.parametrize(
    "args,expected",
    [
        (("restart", "a", "b"), ["a", "b"]),
        (("restart", "--running-only", "a"), ["a"]),
        (("restart", "--timeout", "60", "a"), ["a"]),
        (("restart", "a", "--timeout", 60), ["a"]),
        (("restart", "--timeout=60"), []),
        (("restart", "--timeout", "60"), []),
        (("restart",), []),
    ],
)
def test_restart_names_without_all(mock_list, args, expected):
    assert multipass_cmd._restart_instance_names(args) == expected
    mock_list.assert_not_called()


@pytest.mark.parametrize(
    "args",
    [
        ("restart", "--all"),
        ("restart", "--all", "--running-only"),
        ("restart", "--all", "--timeout", "60"),
    ],
)
def test_restart_all_resolves_names_and_skips_deleted(mock_list, args):
    assert multipass_cmd._restart_instance_names(args) == ["running", "stopped"]
    mock_list.assert_called_once_with("list", "--format=json")
