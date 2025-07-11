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

    def test_launch_stop(self):
        """Verify that taking a snapshot of a non-existent instance fails with
        an appropriate error."""
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
        assert multipass("stop", f"{name}")
        validate_list_output(name, {"state": "Stopped"})

    def test_launch_stop_start(self):
        """Verify that taking a snapshot of a non-existent instance fails with
        an appropriate error."""
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
        """Verify that taking a snapshot of a non-existent instance fails with
        an appropriate error."""
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
