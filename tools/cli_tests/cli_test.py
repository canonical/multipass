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
    multipass,
    shell
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
    assert multipass("stop", f"{name}")
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
    )

    validate_list_output(name, {"state": "Running"})

    with shell(name) as vm_shell:
        vm_shell.expect(r"ubuntu@.*:.*\$", timeout=30)

        # Send a command and expect output
        vm_shell.sendline('echo "Hello from multipass"')
        vm_shell.expect("Hello from multipass")
        vm_shell.expect(r"ubuntu@.*:.*\$")

        # Test another command
        vm_shell.sendline("pwd")
        vm_shell.expect(r"/home/ubuntu")
        vm_shell.expect(r"ubuntu@.*:.*\$")

        # Core count
        vm_shell.sendline("nproc")
        vm_shell.expect("2")
        vm_shell.expect(r"ubuntu@.*:.*\$", timeout=30)

        # User name
        vm_shell.sendline("whoami")
        vm_shell.expect("ubuntu")
        vm_shell.expect(r"ubuntu@.*:.*\$", timeout=30)

        # Hostname
        vm_shell.sendline("hostname")
        vm_shell.expect(f"{name}")
        vm_shell.expect(r"ubuntu@.*:.*\$", timeout=30)

        # Ubuntu Series
        vm_shell.sendline("grep --color=never '^VERSION=' /etc/os-release")
        vm_shell.expect(r'VERSION="22\.04\..* LTS \(Jammy Jellyfish\)"')
        vm_shell.expect(r"ubuntu@.*:.*\$", timeout=30)

        # Exit the shell
        vm_shell.sendline("exit")
        vm_shell.expect(pexpect.EOF)
        vm_shell.wait()

        assert vm_shell.exitstatus == 0

    # Remove the instance.
    assert multipass("delete", f"{name}")
