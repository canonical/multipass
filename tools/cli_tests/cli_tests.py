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

"""Multipass command line e2e tests"""

import pexpect

from cli_tests.utils import (
    is_valid_ipv4_addr,
    uuid4_str,
    validate_list_output,
    validate_info_output,
    build_snapshot_tree,
    collapse_to_snapshot_tree,
    find_lineage,
    file_exists,
    take_snapshot,
    multipass,
)


def test_list_empty():
    """Try to list instances whilst there are none."""
    assert "No instances found." in multipass("list")


def test_list_snapshots_empty():
    """Try to list snapshots whilst there are none."""
    assert "No snapshots found." in multipass("list", "--snapshots")


def test_launch_noble():
    """Try to launch an Ubuntu 24.04 VM with 2 CPUs 1GiB RAM and 6G disk.
    Then, validate the basics."""
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
        "noble",
        retry=3,
        timeout=600,
    )

    validate_list_output(
        name,
        {"state": "Running", "release": "Ubuntu 24.04 LTS", "ipv4": is_valid_ipv4_addr},
    )

    validate_info_output(
        name,
        {
            "cpu_count": "2",
            "snapshot_count": "0",
            "state": "Running",
            "mounts": {},
            "image_release": "24.04 LTS",
            "ipv4": is_valid_ipv4_addr,
        },
    )

    # Try to stop the instance
    assert multipass("stop", f"{name}", timeout=180)
    validate_info_output(name, {"state": "Stopped"})

    # Try to start the instance
    assert multipass("start", f"{name}")
    validate_info_output(name, {"state": "Running"})

    # Remove the instance.
    assert multipass("delete", f"{name}")
    validate_info_output(name, {"state": "Deleted"})


def test_shell():
    """Launch an Ubuntu 22.04 VM with 2 CPUs 1GiB RAM and 6G disk.
    Then, try to shell into it and execute some basic commands."""
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
        timeout=600,
    )

    validate_list_output(name, {"state": "Running"})

    with multipass("shell", f"{name}", interactive=True) as vm_shell:
        vm_shell.expect(r"ubuntu@.*:.*\$", timeout=30)
        # Send a command and expect output
        vm_shell.sendline('echo "Hello from multipass"')
        vm_shell.expect("Hello from multipass")
        vm_shell.expect(r"ubuntu@.*:.*\$")

        # Test another command
        vm_shell.sendline("pwd")
        vm_shell.expect(r"/home/ubuntu")
        vm_shell.expect(r"ubuntu@.*:.*\$")

        # Verify the basics

        # Core count
        vm_shell.sendline("nproc")
        vm_shell.expect("2")

        # User name
        vm_shell.sendline("whoami")
        vm_shell.expect("ubuntu")

        # Hostname
        vm_shell.sendline("hostname")
        vm_shell.expect(f"{name}")

        # Ubuntu Series
        vm_shell.sendline("grep --color=never '^VERSION=' /etc/os-release")
        vm_shell.expect(r'VERSION="22\.04\..* LTS \(Jammy Jellyfish\)"')

        # Exit the shell
        vm_shell.sendline("exit")
        vm_shell.expect(pexpect.EOF)
        vm_shell.wait()

        assert vm_shell.exitstatus == 0

    # Remove the instance.
    assert multipass("delete", f"{name}")


def test_take_snapshot_linear_history():
    """Launch an Ubuntu 22.04 VM with 2 CPUs 1GiB RAM and 6G disk.
    Then, try to test snapshots feature"""
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
        timeout=600,
    )

    validate_list_output(name, {"state": "Running"})
    validate_info_output(name, {"snapshot_count": "0"})
    take_snapshot(name, "snapshot1")
    validate_info_output(name, {"snapshot_count": "1"})
    take_snapshot(name, "snapshot2", "snapshot1")
    validate_info_output(name, {"snapshot_count": "2"})
    take_snapshot(name, "snapshot3", "snapshot2")
    validate_info_output(name, {"snapshot_count": "3"})


def test_take_snapshot_delete_linear_history():
    """Launch an Ubuntu 22.04 VM with 2 CPUs 1GiB RAM and 6G disk.
    Then, try to test snapshots feature"""
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
        timeout=600,
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


def test_take_snapshot_and_restore_linear():
    """Tests snapshot creation and restoration in a Multipass VM.

    Launches a VM, takes a chain of three snapshots, then restores each one
    destructively and verifies file presence and snapshot consistency at each step.
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
        timeout=600,
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


def test_take_snapshot_and_restore_branched():
    """Tests snapshot creation and restoration in a Multipass VM.

    Launches a VM, takes a chain of three snapshots, then restores each one
    destructively and verifies file presence and snapshot consistency at each step.
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


def test_take_snapshot_delete_branched():
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

    with multipass("info", "--format=json", "--snapshots", f"{name}").json() as output:
        result = output.jq(f'.info["{name}"].snapshots')[0]
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

    with multipass("info", "--format=json", "--snapshots", f"{name}").json() as output:
        result = output.jq(f'.info["{name}"].snapshots')[0]
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

    with multipass("info", "--format=json", "--snapshots", f"{name}").json() as output:
        result = output.jq(f'.info["{name}"].snapshots')[0]
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

    with multipass("info", "--format=json", "--snapshots", f"{name}").json() as output:
        result = output.jq(f'.info["{name}"].snapshots')[0]
        expected_snapshot_tree = {
            "snapshot3a": {},
            "snapshot3b": {},
            "snapshot4a": {},
            "snapshot5a": {},
            "snapshot5b": {},
        }
        assert collapse_to_snapshot_tree(result) == expected_snapshot_tree