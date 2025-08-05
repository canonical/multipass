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

from collections import defaultdict

from .multipass_cmd import multipass


def take_snapshot(vm_name, snapshot_name, expected_parent="", expected_comment=""):
    """Create and verify a snapshot of a Multipass VM.

    - Creates a file inside the VM to mark pre-snapshot state.
    - Verifies file presence to confirm the VM is responsive.
    - Stops the VM and takes a snapshot.
    - Asserts the snapshot exists and matches expected metadata.

    Args:
        vm_name (str): Name of the target VM.
        snapshot_name (str): Name to identify the snapshot.
        expected_parent (str, optional): Expected parent snapshot name. Defaults to "".
        expected_comment (str, optional): Expected snapshot comment. Defaults to "".

    Raises:
        AssertionError: If any command fails or snapshot metadata does not match expectations.
    """
    assert multipass("exec", f"{vm_name}", "--", "touch", f"before_{snapshot_name}")
    assert multipass("exec", f"{vm_name}", "--", "ls", f"before_{snapshot_name}")
    assert multipass("stop", f"{vm_name}")

    with multipass("snapshot", f"{vm_name}", "--name", f"{snapshot_name}") as output:
        assert output.exitstatus == 0
        assert "Snapshot taken" in output

    with multipass("list", "--format=json", "--snapshots").json() as output:
        assert output.exitstatus == 0
        assert vm_name in output["info"]
        assert snapshot_name in output["info"][vm_name]
        snapshot = output["info"][vm_name][snapshot_name]
        assert expected_parent == snapshot["parent"]
        assert expected_comment == snapshot["comment"]


def build_snapshot_tree(vm_name, tree, parent=""):
    for name, children in tree.items():
        take_snapshot(vm_name, name, parent)
        build_snapshot_tree(vm_name, children, name)
        if parent != "":
            assert multipass("restore", f"{vm_name}.{parent}", "--destructive")


def collapse_to_snapshot_tree(flat_map):
    tree = {}
    children = defaultdict(dict)
    roots = []

    # First pass: map all nodes to their children
    for name, meta in flat_map.items():
        parent = meta["parent"]
        if parent:
            children[parent][name] = children[name]  # ensures nesting
        else:
            roots.append(name)
            tree[name] = children[name]

    return tree
