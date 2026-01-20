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

"""Multipass command line tests for the snapshot feature."""

import pytest

from cli_tests.utilities import (
    find_lineage,
)

from cli_tests.multipass import (
    build_snapshot_tree,
    collapse_to_snapshot_tree,
    path_exists,
    take_snapshot,
    multipass,
    random_vm_name,
    state,
    snapshot_count,
)


@pytest.mark.snapshot
@pytest.mark.usefixtures("multipassd_class_scoped")
class TestSnapshot:
    """Snapshot tests."""

    def test_take_snapshot_of_nonexistent_instance(self):
        """Verify that taking a snapshot of a non-existent instance fails with
        an appropriate error."""
        name = random_vm_name()
        assert f'instance "{name}" does not exist' in multipass("snapshot", f"{name}")

    def test_try_restore_snapshot_of_nonexistent_instance(self):
        """Verify that restoring a snapshot for a non-existent instance fails
        with an appropriate error."""
        name = random_vm_name()
        assert f'instance "{name}" does not exist' in multipass(
            "restore", f"{name}.snapshot1"
        )

    def test_take_snapshot(self, instance):
        """Ensure that a snapshot can be successfully created for a newly
        launched instance."""

        assert snapshot_count(instance) == 0
        take_snapshot(instance, "snapshot1")
        assert snapshot_count(instance) == 1

    def test_take_snapshot_name_conflict(self, instance):
        """Validate that attempting to create a snapshot with a duplicate name
        fails gracefully."""

        assert snapshot_count(instance) == 0
        take_snapshot(instance, "snapshot1")
        assert snapshot_count(instance) == 1

        with multipass("snapshot", instance, "--name", "snapshot1") as result:
            assert not result
            assert "Snapshot already exists" in result

        assert snapshot_count(instance) == 1

    def test_snapshot_restore_while_running(self, instance):
        """Ensure restoring a snapshot while the instance is running fails with
        a proper error message."""

        assert snapshot_count(instance) == 0
        take_snapshot(instance, "snapshot1")
        assert snapshot_count(instance) == 1
        assert state(instance) == "Stopped"

        assert multipass("start", instance)
        assert state(instance) == "Running"

        with multipass("restore", f"{instance}.snapshot1") as result:
            assert not result
            assert (
                "Multipass can only restore snapshots of stopped instances." in result
            )

    def test_take_snapshot_linear_history(self, instance):
        """Verify that a linear chain of snapshots can be created, each
        referencing the correct parent."""

        assert snapshot_count(instance) == 0
        take_snapshot(instance, "snapshot1")
        assert snapshot_count(instance) == 1
        take_snapshot(instance, "snapshot2", "snapshot1")
        assert snapshot_count(instance) == 2
        take_snapshot(instance, "snapshot3", "snapshot2")
        assert snapshot_count(instance) == 3

    def test_take_snapshot_delete_linear_history(self, instance):
        """Confirm that deleting a snapshot in a linear history reduces the
        snapshot count appropriately."""

        assert snapshot_count(instance) == 0
        take_snapshot(instance, "snapshot1")
        assert snapshot_count(instance) == 1
        take_snapshot(instance, "snapshot2", "snapshot1")
        assert snapshot_count(instance) == 2
        take_snapshot(instance, "snapshot3", "snapshot2")
        assert snapshot_count(instance) == 3

        assert multipass("delete", f"{instance}.snapshot1", "--purge")
        assert snapshot_count(instance) == 2

        with multipass(
            "info", "--format=json", "--snapshots", f"{instance}"
        ).json() as output:
            result = output.jq(f'.info["{instance}"].snapshots').first()
            expected_snapshot_tree = {
                "snapshot2": {
                    "snapshot3": {},
                },
            }
            assert collapse_to_snapshot_tree(result) == expected_snapshot_tree

    @pytest.mark.slow
    def test_take_snapshot_and_restore_linear(self, instance):
        """Test restoring each snapshot in a linear chain and verify file system
        state consistency after each restore."""

        assert snapshot_count(instance) == 0
        take_snapshot(instance, "snapshot1")
        assert snapshot_count(instance) == 1
        take_snapshot(instance, "snapshot2", "snapshot1")
        assert snapshot_count(instance) == 2
        take_snapshot(instance, "snapshot3", "snapshot2")
        assert snapshot_count(instance) == 3

        assert multipass("restore", f"{instance}.snapshot1", "--destructive")
        assert snapshot_count(instance) == 3

        assert path_exists(instance, "before_snapshot1")
        assert not path_exists(instance, "before_snapshot2")
        assert not path_exists(instance, "before_snapshot3")

        assert multipass("stop", instance)

        assert multipass("restore", f"{instance}.snapshot2", "--destructive")
        assert snapshot_count(instance) == 3

        assert path_exists(instance, "before_snapshot1")
        assert path_exists(instance, "before_snapshot2")
        assert not path_exists(instance, "before_snapshot3")

        assert multipass("stop", instance)

        assert multipass("restore", f"{instance}.snapshot3", "--destructive")
        assert snapshot_count(instance) == 3

        assert path_exists(instance, "before_snapshot1")
        assert path_exists(instance, "before_snapshot2")
        assert path_exists(instance, "before_snapshot3")

        assert multipass("stop", instance)

    @pytest.mark.slow
    def test_take_snapshot_and_restore_branched(self, instance):
        """Tests snapshot creation and restoration in a Multipass VM.

        Launches a VM, takes a chain of three snapshots, then restores each one
        destructively and verifies file presence and snapshot consistency at
        each step.
        """

        assert snapshot_count(instance) == 0

        # Snapshot tree structure to build
        snapshot_tree = {
            "snapshot1": {
                "snapshot2a": {
                    "snapshot3a": {},
                },
            },
        }

        build_snapshot_tree(instance, snapshot_tree)
        assert snapshot_count(instance) == 3

        assert multipass("restore", f"{instance}.snapshot1", "--destructive")
        assert snapshot_count(instance) == 3

        assert path_exists(instance, "before_snapshot1")
        assert not path_exists(instance, "before_snapshot2a")
        assert not path_exists(instance, "before_snapshot3a")

        assert multipass("stop", instance)

        assert multipass("restore", f"{instance}.snapshot2a", "--destructive")
        assert snapshot_count(instance) == 3

        assert path_exists(instance, "before_snapshot1")
        assert path_exists(instance, "before_snapshot2a")
        assert not path_exists(instance, "before_snapshot3a")

        assert multipass("stop", instance)

        take_snapshot(instance, "snapshot3b", "snapshot2a")
        assert snapshot_count(instance) == 4

        assert path_exists(instance, "before_snapshot1")
        assert path_exists(instance, "before_snapshot3b")

        assert multipass("stop", instance)

        assert multipass("restore", f"{instance}.snapshot1", "--destructive")
        take_snapshot(instance, "snapshot2b", "snapshot1")
        assert snapshot_count(instance) == 5

    @pytest.mark.slow  # I mean it.
    def test_take_snapshot_delete_branched(self, instance):
        """
        Tests snapshot branching and hierarchy preservation in a Multipass VM.

        This test launches a VM, creates an initial snapshot, and then repeatedly
        restores earlier snapshots to create left and right branches—forming a
        binary-style snapshot tree. Each node spawns two child snapshots via
        destructive restore + snapshot.
        """
        assert snapshot_count(instance) == 0

        # Snapshot tree structure to build
        snapshot_tree = {
            "snapshot1": {
                "snapshot2a": {
                    "snapshot3a": {},
                    "snapshot3b": {},
                },
                "snapshot2b": {
                    "snapshot4a": {},
                    "snapshot4b": {
                        "snapshot5a": {},
                        "snapshot5b": {},
                    },
                },
            },
        }

        build_snapshot_tree(instance, snapshot_tree)
        assert snapshot_count(instance) == 9

        # Delete the common ancestor
        assert multipass("delete", f"{instance}.snapshot1", "--purge")
        assert snapshot_count(instance) == 8

        assert multipass("restore", f"{instance}.snapshot5b", "--destructive")

        # We'll expect all checkpoint files to exist
        lineage = find_lineage(snapshot_tree, "snapshot5b")
        for ancestor in lineage:
            assert path_exists(instance, f"before_{ancestor}")

        assert multipass("stop", instance)

        with multipass(
            "info", "--format=json", "--snapshots", instance
        ).json() as output:
            result = output.jq(f'.info["{instance}"].snapshots').first()
            expected_snapshot_tree = {
                "snapshot2a": {
                    "snapshot3a": {},
                    "snapshot3b": {},
                },
                "snapshot2b": {
                    "snapshot4a": {},
                    "snapshot4b": {
                        "snapshot5a": {},
                        "snapshot5b": {},
                    },
                },
            }
            assert collapse_to_snapshot_tree(result) == expected_snapshot_tree

        assert multipass("delete", f"{instance}.snapshot4b", "--purge")
        assert snapshot_count(instance) == 7

        with multipass(
            "info", "--format=json", "--snapshots", f"{instance}"
        ).json() as output:
            result = output.jq(f'.info["{instance}"].snapshots').first()
            expected_snapshot_tree = {
                "snapshot2a": {
                    "snapshot3a": {},
                    "snapshot3b": {},
                },
                "snapshot2b": {
                    "snapshot4a": {},
                    "snapshot5a": {},
                    "snapshot5b": {},
                },
            }
            assert collapse_to_snapshot_tree(result) == expected_snapshot_tree

        assert multipass("delete", f"{instance}.snapshot2a", "--purge")
        assert snapshot_count(instance) == 6

        with multipass(
            "info", "--format=json", "--snapshots", f"{instance}"
        ).json() as output:
            result = output.jq(f'.info["{instance}"].snapshots').first()
            expected_snapshot_tree = {
                "snapshot3a": {},
                "snapshot3b": {},
                "snapshot2b": {
                    "snapshot4a": {},
                    "snapshot5a": {},
                    "snapshot5b": {},
                },
            }
            assert collapse_to_snapshot_tree(result) == expected_snapshot_tree

        assert multipass("delete", f"{instance}.snapshot2b", "--purge")
        assert snapshot_count(instance) == 5

        with multipass(
            "info", "--format=json", "--snapshots", f"{instance}"
        ).json() as output:
            result = output.jq(f'.info["{instance}"].snapshots').first()
            expected_snapshot_tree = {
                "snapshot3a": {},
                "snapshot3b": {},
                "snapshot4a": {},
                "snapshot5a": {},
                "snapshot5b": {},
            }
            assert collapse_to_snapshot_tree(result) == expected_snapshot_tree
