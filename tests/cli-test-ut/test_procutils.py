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

"""Unit tests for cli.utilities.procutils."""

from unittest.mock import MagicMock

import pytest

from cli.utilities import procutils


class TestSendCtrlC:
    """Tests for send_ctrl_c()."""

    def test_uses_sys_executable_with_argv_list_on_windows(self, monkeypatch):
        monkeypatch.setattr(procutils.sys, "platform", "win32")
        monkeypatch.setattr(procutils.sys, "executable", r"C:\Program Files\Python\python.exe")
        mock_run = MagicMock()
        monkeypatch.setattr(procutils.subprocess, "run", mock_run)

        procutils.send_ctrl_c(1234)

        mock_run.assert_called_once()
        args, kwargs = mock_run.call_args
        argv = args[0]
        assert isinstance(argv, list)
        assert argv[0] == r"C:\Program Files\Python\python.exe"
        assert argv[1] == "-c"
        assert "AttachConsole(1234)" in argv[2]
        assert "GenerateConsoleCtrlEvent(0, 0)" in argv[2]
        assert kwargs == {"shell": False}

    def test_raises_not_implemented_on_non_windows(self, monkeypatch):
        monkeypatch.setattr(procutils.sys, "platform", "linux")

        with pytest.raises(NotImplementedError):
            procutils.send_ctrl_c(1234)
