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

"""Multipass command line tests for the `restart` command."""

import pytest

from cli.multipass import multipass, state, launch, get_boot_id


@pytest.mark.restart
@pytest.mark.usefixtures("multipassd")
class TestRestart:
    """`multipass restart` tests."""

    def test_restart_all_with_mixed_states(self):
        """`multipass restart --all` succeeds across running and stopped instances."""
        with (
            launch({"autopurge": False}) as running_name,
            launch({"autopurge": False}) as stopped_name,
        ):
            assert state(running_name) == "Running"

            assert multipass("stop", stopped_name)
            assert state(stopped_name) == "Stopped"

            running_boot_id_before = get_boot_id(running_name)

            assert multipass("restart", running_name, stopped_name)

            assert state(running_name) == "Running"
            assert state(stopped_name) == "Running"

            assert get_boot_id(running_name) != running_boot_id_before

            assert multipass("delete", running_name, stopped_name)
            assert state(running_name) == "Deleted" and state(stopped_name) == "Deleted"
            assert multipass("purge")
