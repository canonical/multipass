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

# pytest -s tools/cli_tests/cli_tests.py --build-root build/bin/ --data-root=/tmp/multipass-test --remove-all-instances -vv
# kill zombie dnsmasq: ps -eo pid,cmd | awk '/dnsmasq/ && /mpqemubr0/ { print $1 }' | xargs sudo kill -9

import sys

from collections.abc import Sequence

import pexpect

from cli_tests.conftest import multipass
from cli_tests.utils import (
    is_valid_ipv4_addr,
    retry,
    uuid4_str,
)


def verify_vm(name, properties):
    """Validate a VM's properties by fetching it via the `list`
    command. The properties are compared by the actual values.
    """
    with multipass("list", "--format=json").json() as output:
        assert output.exitstatus == 0
        result = output.jq(f'.list[] | select(.name=="{name}")')
        assert len(result) == 1
        [instance] = result
        for k, v in properties.items():
            assert k in instance
            # A property can be assigned to a callable. In that case, we'd want
            # to treat it as a predicate and call it. We'd call it for each
            # element if the matching key's value is a collection.
            if callable(v):
                if isinstance(instance[k], Sequence):
                    for item in instance[k]:
                        assert v(item)
                else:
                    assert v(instance[k])
            else:
                assert instance[k] == v


def file_exists(vm_name, path):
    return multipass("exec", f"{vm_name}", "--", "ls", f"{path}", timeout=180)


def take_snapshot_and_verify(
    vm_name, snapshot_name, expected_parent="", expected_comment=""
):
    assert multipass(
        "exec",
        f"{vm_name}",
        "--",
        "touch",
        f"before_{snapshot_name}",
        timeout=180,
    )

    assert multipass(
        "exec",
        f"{vm_name}",
        "--",
        "ls",
        f"before_{snapshot_name}",
        timeout=180,
    )

    assert multipass("stop", f"{vm_name}")

    with multipass("snapshot", f"{vm_name}") as output:
        assert output.exitstatus == 0
        assert "Snapshot taken" in output

    with multipass("list", "--format=json", "--snapshots").json() as output:
        assert output.exitstatus == 0
        assert vm_name in output["info"]
        assert snapshot_name in output["info"][vm_name]
        snapshot = output["info"][vm_name][snapshot_name]
        assert expected_parent == snapshot["parent"]
        assert expected_comment == snapshot["comment"]


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

    # Launch with retry.
    # Sometimes the daemon is not immediately ready. Retry attempts
    # are here to remedy that until we have a "multipass status" command.

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

    verify_vm(
        name,
        {"state": "Running", "release": "Ubuntu 24.04 LTS", "ipv4": is_valid_ipv4_addr},
    )

    # Verify that list contains the instance
    with multipass("list", "--format=json").json() as output:
        assert output.exitstatus == 0
        result = output.jq(f'.list[] | select(.name=="{name}")')
        assert len(result) == 1
        [instance] = result
        assert instance["release"] == "Ubuntu 24.04 LTS"
        assert instance["state"] == "Running"
        assert is_valid_ipv4_addr(instance["ipv4"][0])

    # Verify the instance info
    with multipass("info", "--format=json", f"{name}").json() as output:
        assert output.exitstatus == 0
        assert "errors" in output
        assert output["errors"] == []
        assert "info" in output
        assert name in output["info"]
        instance_info = output["info"][name]
        assert instance_info["cpu_count"] == "2"
        assert instance_info["snapshot_count"] == "0"
        assert instance_info["state"] == "Running"
        assert instance_info["mounts"] == {}
        assert instance_info["image_release"] == "24.04 LTS"
        assert is_valid_ipv4_addr(instance_info["ipv4"][0])

    # Try to stop the instance
    assert multipass("stop", f"{name}", timeout=180)

    # Verify the instance info
    with multipass("info", "--format=json", f"{name}").json() as output:
        assert output.exitstatus == 0
        assert "errors" in output
        assert output["errors"] == []
        assert "info" in output
        assert name in output["info"]
        instance_info = output["info"][name]
        assert instance_info["state"] == "Stopped"

    # Try to start the instance
    assert multipass("start", f"{name}")

    with multipass("info", "--format=json", f"{name}").json() as output:
        assert output.exitstatus == 0
        assert "errors" in output
        assert output["errors"] == []
        assert "info" in output
        assert name in output["info"]
        instance_info = output["info"][name]
        assert instance_info["state"] == "Running"

    with multipass("info", "--format=json", f"{name}", retry=3).json() as output:
        assert output.exitstatus == 0
        assert "errors" in output
        assert output["errors"] == []
        assert "info" in output
        assert name in output["info"]
        instance_info = output["info"][name]
        assert instance_info["state"] == "Running"

    # Remove the instance.
    assert multipass("delete", f"{name}")


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

    verify_vm(name, {"state": "Running"})

    with multipass("shell", f"{name}", encoding="utf-8", interactive=True) as vm_shell:
        # Display the contents
        vm_shell.logfile = sys.stdout.buffer
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
    assert multipass("delete", f"{name}").exitstatus == 0


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

    verify_vm(name, {"state": "Running"})

    take_snapshot_and_verify(name, "snapshot1")
    take_snapshot_and_verify(name, "snapshot2", "snapshot1")
    take_snapshot_and_verify(name, "snapshot3", "snapshot2")


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

    verify_vm(name, {"state": "Running"})

    take_snapshot_and_verify(name, "snapshot1")
    take_snapshot_and_verify(name, "snapshot2", "snapshot1")
    take_snapshot_and_verify(name, "snapshot3", "snapshot2")
    assert multipass("delete", f"{name}.snapshot1", "--purge")


def test_take_snapshot_and_restore():
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

    verify_vm(name, {"state": "Running"})

    take_snapshot_and_verify(name, "snapshot1")
    take_snapshot_and_verify(name, "snapshot2", "snapshot1")
    take_snapshot_and_verify(name, "snapshot3", "snapshot2")

    assert multipass("restore", f"{name}.snapshot1", "--destructive")

    assert file_exists(name, "before_snapshot1")
    assert not file_exists(name, "before_snapshot2")
    assert not file_exists(name, "before_snapshot3")
