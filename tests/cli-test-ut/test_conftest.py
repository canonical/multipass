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

"""Unit tests for fixtures in cli.conftest."""

from unittest.mock import MagicMock, call

from cli import conftest


class TestWindowsPrivilegedMounts:
    """Tests for the windows_privileged_mounts fixture."""

    def test_waits_for_restart_after_enabling(self, monkeypatch):
        monkeypatch.setattr(conftest.sys, "platform", "win32")
        mock_multipass = MagicMock(side_effect=["false\n", True])
        monkeypatch.setattr(conftest, "multipass", mock_multipass)
        mock_daemon = MagicMock()

        conftest.windows_privileged_mounts.__wrapped__(mock_daemon)

        assert mock_multipass.call_args_list == [
            call("get", "local.privileged-mounts"),
            call("set", "local.privileged-mounts=1"),
        ]
        mock_daemon.wait_for_restart.assert_called_once_with()

    def test_does_not_wait_when_already_enabled(self, monkeypatch):
        monkeypatch.setattr(conftest.sys, "platform", "win32")
        mock_multipass = MagicMock(return_value="true\n")
        monkeypatch.setattr(conftest, "multipass", mock_multipass)
        mock_daemon = MagicMock()

        conftest.windows_privileged_mounts.__wrapped__(mock_daemon)

        mock_multipass.assert_called_once_with("get", "local.privileged-mounts")
        mock_daemon.wait_for_restart.assert_not_called()

    def test_skips_wait_when_multipassd_is_none(self, monkeypatch):
        monkeypatch.setattr(conftest.sys, "platform", "win32")
        mock_multipass = MagicMock(side_effect=["false\n", True])
        monkeypatch.setattr(conftest, "multipass", mock_multipass)

        conftest.windows_privileged_mounts.__wrapped__(None)

        assert mock_multipass.call_args_list == [
            call("get", "local.privileged-mounts"),
            call("set", "local.privileged-mounts=1"),
        ]

    def test_noop_on_non_windows(self, monkeypatch):
        monkeypatch.setattr(conftest.sys, "platform", "linux")
        mock_multipass = MagicMock()
        monkeypatch.setattr(conftest, "multipass", mock_multipass)
        mock_daemon = MagicMock()

        conftest.windows_privileged_mounts.__wrapped__(mock_daemon)

        mock_multipass.assert_not_called()
        mock_daemon.wait_for_restart.assert_not_called()
