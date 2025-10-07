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

"""Multipass command line tests for the `authenticate` command."""

import sys

import pytest

from cli_tests.multipass import multipass
from cli_tests.utilities import TempDirectory

from cli_tests.config import config


@pytest.mark.authenticate
@pytest.mark.usefixtures("multipassd")
class TestAuthenticate:
    """CLI authentication behavior tests."""

    def test_authenticate(self, multipassd):
        assert multipass("set", "local.passphrase=therewillbesecrets")
        multipassd.wait_for_restart()

        with TempDirectory() as empty_home_dir:
            # Authentication must fail since we're using an empty dif as HOME.

            if config.daemon_controller == "standalone" and sys.platform != "win32":
                # Overriding home for snap is convoluted, and QT in Windows does not
                # respect APPDATA overrides so this approach only works on platforms
                # that respect HOME.
                assert "Please authenticate" in multipass(
                    "list", env={"HOME": str(empty_home_dir)}
                )

            # Invalid password.
            assert "Passphrase is not correct" in multipass(
                "authenticate", "therewillbesecretz", env={"HOME": str(empty_home_dir)}
            )

            # Will succeed
            assert multipass(
                "authenticate", "therewillbesecrets", env={"HOME": str(empty_home_dir)}
            )

            # This will not.
            assert "No instances found." in multipass(
                "list", env={"HOME": str(empty_home_dir)}
            )
