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

from cli_tests.utils import read_output, uuid4_str, is_valid_ipv4_addr, retry


def test_list_empty(multipassd, multipass):
    """Try to list instances whilst there are none."""
    with read_output(multipass("list")) as output:
        assert "No instances found." in output


def test_list_snapshots_empty(multipassd, multipass):
    """Try to list snapshots whilst there are none."""
    with read_output(multipass("list", "--snapshots")) as output:
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
        with read_output(
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
    with read_output(multipass("list", "--format=json")) as output:
        assert output.returncode == 0
        result = output.jq(f'.list[] | select(.name=="{name}")')
        assert len(result) == 1
        [instance] = result
        assert instance["release"] == "Ubuntu 24.04 LTS"
        assert instance["state"] == "Running"
        assert is_valid_ipv4_addr(instance["ipv4"][0])

    # Verify the instance info
    with read_output(multipass("info", "--format=json", f"{name}")) as output:
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
    with read_output(multipass("stop", f"{name}"), timeout=180) as output:
        assert output.returncode == 0

    # Verify the instance info
    with read_output(multipass("info", "--format=json", f"{name}")) as output:
        assert output.returncode == 0
        assert "errors" in output
        assert output["errors"] == []
        assert "info" in output
        assert name in output["info"]
        instance_info = output["info"][name]
        assert instance_info["state"] == "Stopped"

    # Try to start the instance
    with read_output(multipass("start", f"{name}"), timeout=180) as output:
        assert output.returncode == 0

    with read_output(multipass("info", "--format=json", f"{name}")) as output:
        assert output.returncode == 0
        assert "errors" in output
        assert output["errors"] == []
        assert "info" in output
        assert name in output["info"]
        instance_info = output["info"][name]
        assert instance_info["state"] == "Running"

    # Remove the instance.
    with read_output(multipass("delete", f"{name}"), timeout=180) as output:
        assert output.returncode == 0


def test_shell(multipassd, multipass):
    """Launch an Ubuntu 22.04 VM with 2 CPUs 1GiB RAM and 6G disk.
    Then, try to shell into it and execute some basic commands."""
    name = uuid4_str("instance")
    timeout = 600  # 10 minutes should be plenty enough.

    @retry(retries=3, delay=2.0)
    def launch_vm():
        with read_output(
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

    # Remove the instance.
    with read_output(multipass("delete", f"{name}"), timeout=180) as output:
        assert output.returncode == 0
