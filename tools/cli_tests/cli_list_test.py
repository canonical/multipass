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

"""Multipass command line tests for the list CLI command."""

import pytest

from cli_tests.multipass import (
    multipass,
)


@pytest.mark.list
class TestList:
    """List tests."""

    def test_list_empty(self):
        """Try to list instances whilst there are none."""
        assert "No instances found." in multipass("list")

    def test_list_snapshots_empty(self):
        """Try to list snapshots whilst there are none."""
        assert "No snapshots found." in multipass("list", "--snapshots")
