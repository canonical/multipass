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
    uuid4_str,
    find_lineage,
)

from cli_tests.multipass import (
    validate_info_output,
    validate_list_output,
    build_snapshot_tree,
    collapse_to_snapshot_tree,
    file_exists,
    take_snapshot,
    multipass
)

@pytest.mark.snapshot
@pytest.mark.usefixtures("multipassd")
class TestSnapshot:
    """Snapshot tests."""

    def test_take_snapshot_of_nonexistent_instance(self):
        """Verify that taking a snapshot of a non-existent instance fails with
        an appropriate error."""
        name = uuid4_str("instance")
        assert f'instance "{name}" does not exist' in multipass("snapshot", f"{name}")

    def test_try_restore_snapshot_of_nonexistent_instance(self):
        """Verify that restoring a snapshot for a non-existent instance fails
        with an appropriate error."""
        name = uuid4_str("instance")
        assert f'instance "{name}" does not exist' in multipass(
            "restore", f"{name}.snapshot1"
        )

    def test_take_snapshot(self):
        """Ensure that a snapshot can be successfully created for a newly
        launched instance."""
        name = uuid4_str("instance")

        assert multipass(
            "launch",
            "--cpus",
            "2",
            "--memory",
            "1G",
            "--disk",
            "6G",
            "--name",
            name,
            retry=3,
        )

        validate_list_output(name, {"state": "Running"})
        validate_info_output(name, {"snapshot_count": "0"})
        take_snapshot(name, "snapshot1")
        validate_info_output(name, {"snapshot_count": "1"})

    def test_take_snapshot_name_conflict(self):
        """Validate that attempting to create a snapshot with a duplicate name
        fails gracefully."""
        name = uuid4_str("instance")

        assert multipass(
            "launch",
            "--cpus",
            "2",
            "--memory",
            "1G",
            "--disk",
            "6G",
            "--name",
            name,
            "jammy",
            retry=3,
        )

        validate_list_output(name, {"state": "Running"})
        validate_info_output(name, {"snapshot_count": "0"})
        take_snapshot(name, "snapshot1")
        validate_info_output(name, {"snapshot_count": "1"})

        with multipass("snapshot", f"{name}", "--name", "snapshot1") as result:
            assert not result
            assert "Snapshot already exists" in result

        validate_info_output(name, {"snapshot_count": "1"})

    def test_snapshot_restore_while_running(self):
        """Ensure restoring a snapshot while the instance is running fails with
        a proper error message."""
        name = uuid4_str("instance")

        assert multipass(
            "launch",
            "--cpus",
            "2",
            "--memory",
            "1G",
            "--disk",
            "6G",
            "--name",
            name,
            "jammy",
            retry=3,
        )

        validate_list_output(name, {"state": "Running"})
        validate_info_output(name, {"snapshot_count": "0"})
        take_snapshot(name, "snapshot1")
        validate_info_output(name, {"snapshot_count": "1"})

        assert multipass("start", f"{name}")
        validate_info_output(name, {"state": "Running"})

        with multipass("restore", f"{name}.snapshot1") as result:
            assert not result
            assert (
                "Multipass can only restore snapshots of stopped instances." in result
            )

    def test_take_snapshot_linear_history(self):
        """Verify that a linear chain of snapshots can be created, each
        referencing the correct parent."""
        name = uuid4_str("instance")

        assert multipass(
            "launch",
            "--cpus",
            "2",
            "--memory",
            "1G",
            "--disk",
            "6G",
            "--name",
            name,
            "jammy",
            retry=3,
        )

        validate_list_output(name, {"state": "Running"})
        validate_info_output(name, {"snapshot_count": "0"})
        take_snapshot(name, "snapshot1")
        validate_info_output(name, {"snapshot_count": "1"})
        take_snapshot(name, "snapshot2", "snapshot1")
        validate_info_output(name, {"snapshot_count": "2"})
        take_snapshot(name, "snapshot3", "snapshot2")
        validate_info_output(name, {"snapshot_count": "3"})

    def test_take_snapshot_delete_linear_history(self):
        """Confirm that deleting a snapshot in a linear history reduces the
        snapshot count appropriately."""
        name = uuid4_str("instance")

        assert multipass(
            "launch",
            "--cpus",
            "2",
            "--memory",
            "1G",
            "--disk",
            "6G",
            "--name",
            name,
            "jammy",
            retry=3,
        )

        validate_list_output(name, {"state": "Running"})
        validate_info_output(name, {"snapshot_count": "0"})

        take_snapshot(name, "snapshot1")
        validate_info_output(name, {"snapshot_count": "1"})

        take_snapshot(name, "snapshot2", "snapshot1")
        validate_info_output(name, {"snapshot_count": "2"})

        take_snapshot(name, "snapshot3", "snapshot2")
        validate_info_output(name, {"snapshot_count": "3"})

        assert multipass("delete", f"{name}.snapshot1", "--purge")
        validate_info_output(name, {"snapshot_count": "2"})
        # TODO: Validate the parent_

    @pytest.mark.slow
    def test_take_snapshot_and_restore_linear(self):
        """Test restoring each snapshot in a linear chain and verify file system
        state consistency after each restore."""
        name = uuid4_str("instance")

        assert multipass(
            "launch",
            "--cpus",
            "2",
            "--memory",
            "1G",
            "--disk",
            "6G",
            "--name",
            name,
            "jammy",
            retry=3,
        )

        validate_list_output(name, {"state": "Running"})
        validate_info_output(name, {"snapshot_count": "0"})

        take_snapshot(name, "snapshot1")
        validate_info_output(name, {"snapshot_count": "1"})

        take_snapshot(name, "snapshot2", "snapshot1")
        validate_info_output(name, {"snapshot_count": "2"})

        take_snapshot(name, "snapshot3", "snapshot2")
        validate_info_output(name, {"snapshot_count": "3"})

        assert multipass("restore", f"{name}.snapshot1", "--destructive")
        validate_info_output(name, {"snapshot_count": "3"})

        assert file_exists(name, "before_snapshot1")
        assert not file_exists(name, "before_snapshot2")
        assert not file_exists(name, "before_snapshot3")

        assert multipass("stop", f"{name}")

        assert multipass("restore", f"{name}.snapshot2", "--destructive")
        validate_info_output(name, {"snapshot_count": "3"})

        assert file_exists(name, "before_snapshot1")
        assert file_exists(name, "before_snapshot2")
        assert not file_exists(name, "before_snapshot3")

        assert multipass("stop", f"{name}")

        assert multipass("restore", f"{name}.snapshot3", "--destructive")
        validate_info_output(name, {"snapshot_count": "3"})

        assert file_exists(name, "before_snapshot1")
        assert file_exists(name, "before_snapshot2")
        assert file_exists(name, "before_snapshot3")

        assert multipass("stop", f"{name}")

    @pytest.mark.slow
    def test_take_snapshot_and_restore_branched(self):
        """Tests snapshot creation and restoration in a Multipass VM.

        Launches a VM, takes a chain of three snapshots, then restores each one
        destructively and verifies file presence and snapshot consistency at
        each step.
        """
        name = uuid4_str("instance")

        assert multipass(
            "launch",
            "--cpus",
            "2",
            "--memory",
            "1G",
            "--disk",
            "6G",
            "--name",
            name,
            "jammy",
            retry=3,
        )

        validate_list_output(name, {"state": "Running"})
        validate_info_output(name, {"snapshot_count": "0"})

        # Snapshot tree structure to build
        snapshot_tree = {
            "snapshot1": {
                "snapshot2a": {
                    "snapshot3a": {},
                },
            },
        }

        build_snapshot_tree(name, snapshot_tree)
        validate_info_output(name, {"snapshot_count": "3"})

        assert multipass("restore", f"{name}.snapshot1", "--destructive")
        validate_info_output(name, {"snapshot_count": "3"})

        assert file_exists(name, "before_snapshot1")
        assert not file_exists(name, "before_snapshot2a")
        assert not file_exists(name, "before_snapshot3a")

        assert multipass("stop", f"{name}")

        assert multipass("restore", f"{name}.snapshot2a", "--destructive")
        validate_info_output(name, {"snapshot_count": "3"})

        assert file_exists(name, "before_snapshot1")
        assert file_exists(name, "before_snapshot2a")
        assert not file_exists(name, "before_snapshot3a")

        assert multipass("stop", f"{name}")

        take_snapshot(name, "snapshot3b", "snapshot2a")
        validate_info_output(name, {"snapshot_count": "4"})

        assert file_exists(name, "before_snapshot1")
        assert file_exists(name, "before_snapshot3b")

        assert multipass("stop", f"{name}")

        assert multipass("restore", f"{name}.snapshot1", "--destructive")
        take_snapshot(name, "snapshot2b", "snapshot1")
        validate_info_output(name, {"snapshot_count": "5"})

    @pytest.mark.slow  # I mean it.
    def test_take_snapshot_delete_branched(self):
        """
        Tests snapshot branching and hierarchy preservation in a Multipass VM.

        This test launches a VM, creates an initial snapshot, and then repeatedly
        restores earlier snapshots to create left and right branches—forming a
        binary-style snapshot tree. Each node spawns two child snapshots via
        destructive restore + snapshot.
        """
        name = uuid4_str("instance")

        assert multipass(
            "launch",
            "--cpus",
            "2",
            "--memory",
            "1G",
            "--disk",
            "6G",
            "--name",
            name,
            "jammy",
            retry=3,
        )

        validate_list_output(name, {"state": "Running"})
        validate_info_output(name, {"snapshot_count": "0"})

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

        build_snapshot_tree(name, snapshot_tree)
        validate_info_output(name, {"snapshot_count": "9"})

        # Delete the common ancestor
        assert multipass("delete", f"{name}.snapshot1", "--purge")
        validate_info_output(name, {"snapshot_count": "8"})

        assert multipass("restore", f"{name}.snapshot5b", "--destructive")

        # We'll expect all checkpoint files to exist
        lineage = find_lineage(snapshot_tree, "snapshot5b")
        for ancestor in lineage:
            assert file_exists(name, f"before_{ancestor}")

        assert multipass("stop", f"{name}")

        with multipass(
            "info", "--format=json", "--snapshots", f"{name}"
        ).json() as output:
            result = output.jq(f'.info["{name}"].snapshots').first()
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

        assert multipass("delete", f"{name}.snapshot4b", "--purge")
        validate_info_output(name, {"snapshot_count": "7"})

        with multipass(
            "info", "--format=json", "--snapshots", f"{name}"
        ).json() as output:
            result = output.jq(f'.info["{name}"].snapshots').first()
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

        assert multipass("delete", f"{name}.snapshot2a", "--purge")
        validate_info_output(name, {"snapshot_count": "6"})

        with multipass(
            "info", "--format=json", "--snapshots", f"{name}"
        ).json() as output:
            result = output.jq(f'.info["{name}"].snapshots').first()
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

        assert multipass("delete", f"{name}.snapshot2b", "--purge")
        validate_info_output(name, {"snapshot_count": "5"})

        with multipass(
            "info", "--format=json", "--snapshots", f"{name}"
        ).json() as output:
            result = output.jq(f'.info["{name}"].snapshots').first()
            expected_snapshot_tree = {
                "snapshot3a": {},
                "snapshot3b": {},
                "snapshot4a": {},
                "snapshot5a": {},
                "snapshot5b": {},
            }
            assert collapse_to_snapshot_tree(result) == expected_snapshot_tree
