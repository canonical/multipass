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

"""Multipass command line tests for the snapshots CLI command."""

import pytest

from cli.multipass import (
    assert_list_snapshots_matches_snapshots,
    multipass,
)


@pytest.mark.snapshots
@pytest.mark.usefixtures("multipassd_class_scoped")
class TestSnapshots:
    """Snapshots tests."""

    def test_snapshots_empty(self):
        """Try to list snapshots whilst there are none."""
        assert "No snapshots found." in multipass("snapshots")

    def test_snapshots_lists_snapshot(self, instance):
        """Ensure a taken snapshot is listed with its parent and comment in every format."""
        assert multipass("stop", instance)
        assert multipass(
            "snapshot", instance, "--name", "snapshot1", "--comment", "first"
        )
        assert multipass(
            "snapshot", instance, "--name", "snapshot2", "--comment", "second"
        )

        with multipass("snapshots", "--format=json").json() as output:
            assert output.exitstatus == 0
            assert output["info"] == {
                instance: {
                    "snapshot1": {"parent": "", "comment": "first"},
                    "snapshot2": {"parent": "snapshot1", "comment": "second"},
                }
            }

        assert_list_snapshots_matches_snapshots()

    # TODO@deprecations remove along with `list --snapshots`
    def test_list_snapshots_matches_snapshots_when_empty(self):
        """Ensure the deprecated `list --snapshots` behaves like `snapshots` when empty."""
        assert_list_snapshots_matches_snapshots()
