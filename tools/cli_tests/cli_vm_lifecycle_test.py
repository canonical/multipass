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

"""Multipass command line tests for the VM lifecycle."""

import pytest

from cli_tests.utils import (
    uuid4_str,
    validate_list_output,
    multipass,
    state,
)


@pytest.mark.lifecycle
class TestVmLifecycle:
    """Virtual machine lifecycle tests."""

    def test_launch_invalid_name(self):
        invalid_names = [
            "1nvalid-name",
            "invalid.name",
            "invalid$name",
            "invalid(name)",
            "invalid-name-",
        ]

        for name in invalid_names:
            with multipass(
                "launch",
                "--cpus",
                "2",
                "--memory",
                "1G",
                "--disk",
                "6G",
                "--name",
                name,
            ) as output:
                assert not output
                assert "Invalid instance name supplied" in output

    def test_lifecycle_ops_on_nonexistent(self):
        name = uuid4_str("instance")
        ops = [
            "start",
            "suspend",
            "stop",
            "delete",
            "recover",
            "restart",
        ]

        for op in ops:
            with multipass(op, f"{name}") as output:
                assert not output
                assert "does not exist" in output

    def test_launch_invalid_ram(self):
        name = uuid4_str("instance")
        with multipass(
            "launch",
            "--cpus",
            "2",
            "--memory",
            "1CiG",
            "--disk",
            "6G",
            "--name",
            name,
        ) as output:
            assert not output
            assert "1CiG is not a valid memory size" in output

    # TODO: Launch with custom cloud-init

    def test_launch_stop(self):
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

        assert state(f"{name}") == "Running"
        assert multipass("stop", f"{name}")
        assert state(f"{name}") == "Stopped"

    def test_launch_stop_start(self):
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

        assert state(f"{name}") == "Running"
        assert multipass("stop", f"{name}")
        assert state(f"{name}") == "Stopped"
        assert multipass("start", f"{name}")
        assert state(f"{name}") == "Running"

    def test_launch_suspend_resume(self):
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

        assert state(f"{name}") == "Running"
        assert multipass("suspend", f"{name}")
        assert state(f"{name}") == "Suspended"
        assert multipass("start", f"{name}")
        assert state(f"{name}") == "Running"

    def test_launch_delete_recover_purge(self):
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

        assert state(f"{name}") == "Running"
        assert multipass("delete", f"{name}")
        assert state(f"{name}") == "Deleted"

        with multipass("start", f"{name}") as output:
            assert not output
            assert f"Instance '{name}' is deleted." in output

        assert multipass("recover", f"{name}")
        assert state(f"{name}") == "Stopped"

        assert multipass("start", f"{name}")

        assert multipass("delete", f"{name}")
        assert state(f"{name}") == "Deleted"

        assert multipass("delete", f"{name}", "--purge")
        with multipass("info", f"{name}") as output:
            assert not output
            assert "does not exist" in output

    def test_launch_delete_purge(self):
        name1 = uuid4_str("instance")
        name2 = uuid4_str("instance")

        assert multipass(
            "launch",
            "--cpus",
            "2",
            "--memory",
            "1G",
            "--disk",
            "6G",
            "--name",
            name1,
            retry=3,
        )

        assert multipass(
            "launch",
            "--cpus",
            "2",
            "--memory",
            "1G",
            "--disk",
            "6G",
            "--name",
            name2,
            retry=3,
        )

        assert multipass("delete", f"{name1}")
        assert state(f"{name1}") == "Deleted"

        assert multipass("delete", f"{name2}")
        assert state(f"{name2}") == "Deleted"

        assert multipass("purge")

        for instance in [name1, name2]:
            with multipass("info", f"{instance}") as output:
                assert not output
                assert "does not exist" in output
