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

"""Unit tests for instance helpers in cli.multipass.helpers."""

import importlib
from pathlib import PurePosixPath

import pytest

from cli.multipass.cmd_output import Output

helpers_module = importlib.import_module("cli.multipass.helpers")


@pytest.fixture
def exec_result(monkeypatch):
    calls = []

    def install(result):
        def fake_multipass(*args, **kwargs):
            calls.append(args)
            return result

        monkeypatch.setattr(helpers_module, "multipass", fake_multipass)
        return calls

    return install


class TestPathExists:
    def test_existing_paths(self, exec_result):
        calls = exec_result(Output("a\nb", 0))

        assert helpers_module.path_exists("vm", "a", PurePosixPath("dir") / "b")
        assert calls == [("exec", "vm", "--", "ls", "-d", "--", "a", "dir/b")]

    def test_missing_path(self, exec_result):
        exec_result(Output("ls: cannot access 'b': No such file or directory", 2))

        assert not helpers_module.path_exists("vm", "b")

    def test_failed_check_is_not_reported_as_missing(self, exec_result):
        exec_result(Output("exec failed: ssh connection refused", 2))

        with pytest.raises(AssertionError, match="Could not check .*ssh connection refused"):
            helpers_module.path_exists("vm", "b")
