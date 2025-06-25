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

import pytest
import time
import re
import sys
import pexpect

from cli_tests.utils import (
    uuid4_str,
    is_valid_ipv4_addr,
    retry,
    expect_json,
    expect_text,
)


def test_list_empty(multipassd, multipass):
    """Try to list instances whilst there are none."""
    with expect_text(multipass("list")) as output:
        assert "No instances found." in output


def test_list_snapshots_empty(multipassd, multipass):
    """Try to list snapshots whilst there are none."""
    with expect_text(multipass("list", "--snapshots")) as output:
        assert "No snapshots found." in output


def test_launch_noble(multipassd, multipass):
    """Try to launch an Ubuntu 24.04 VM with 2 CPUs 1GiB RAM and 6G disk.
    Then, validate the basics."""
    name = uuid4_str("instance")
    launch_timeout = 600  # 10 minutes should be plenty enough.

    # Launch with retry.
    # Sometimes the daemon is not immediately ready. Retry attempts
    # are here to remedy that until we have a "multipass status" command.
    @retry(retries=3, delay=2.0)
    def launch_vm():
        with expect_text(
            multipass(
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
            ),
            timeout=launch_timeout,
        ) as output:
            return output

    assert launch_vm().returncode == 0

    # Verify that list contains the instance
    with expect_json(multipass("list", "--format=json")) as output:
        assert output.returncode == 0
        result = output.jq(f'.list[] | select(.name=="{name}")')
        assert len(result) == 1
        [instance] = result
        assert instance["release"] == "Ubuntu 24.04 LTS"
        assert instance["state"] == "Running"
        assert is_valid_ipv4_addr(instance["ipv4"][0])

    # Verify the instance info
    with expect_json(multipass("info", "--format=json", f"{name}")) as output:
        assert output.returncode == 0
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
    with expect_text(multipass("stop", f"{name}"), timeout=180) as output:
        assert output.returncode == 0

    # Verify the instance info
    with expect_json(multipass("info", "--format=json", f"{name}")) as output:
        assert output.returncode == 0
        assert "errors" in output
        assert output["errors"] == []
        assert "info" in output
        assert name in output["info"]
        instance_info = output["info"][name]
        assert instance_info["state"] == "Stopped"

    # Try to start the instance
    with expect_text(multipass("start", f"{name}")) as output:
        assert output.returncode == 0

    with expect_json(multipass("info", "--format=json", f"{name}")) as output:
        assert output.returncode == 0
        assert "errors" in output
        assert output["errors"] == []
        assert "info" in output
        assert name in output["info"]
        instance_info = output["info"][name]
        assert instance_info["state"] == "Running"

    with expect_json(multipass("info", "--format=json", f"{name}")) as output:
        assert output.returncode == 0
        assert "errors" in output
        assert output["errors"] == []
        assert "info" in output
        assert name in output["info"]
        instance_info = output["info"][name]
        assert instance_info["state"] == "Running"

    # Remove the instance.
    with expect_text(multipass("delete", f"{name}"), timeout=180) as output:
        assert output.returncode == 0


def test_shell(multipassd, multipass):
    """Launch an Ubuntu 22.04 VM with 2 CPUs 1GiB RAM and 6G disk.
    Then, try to shell into it and execute some basic commands."""
    name = uuid4_str("instance")
    timeout = 600  # 10 minutes should be plenty enough.

    @retry(retries=3, delay=2.0)
    def launch_vm():
        with expect_text(
            multipass(
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
            ),
            timeout=timeout,
        ) as output:
            return output

    assert launch_vm().returncode == 0

    with multipass("shell", f"{name}", encoding="utf-8") as vm_shell:
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
    with expect_text(multipass("delete", f"{name}"), timeout=180) as output:
        assert output.returncode == 0
