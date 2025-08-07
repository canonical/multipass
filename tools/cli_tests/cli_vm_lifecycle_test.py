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

from cli_tests.utilities import uuid4_str

from cli_tests.multipass import multipass, state, launch


@pytest.mark.lifecycle
@pytest.mark.usefixtures("multipassd")
class TestVmLifecycle:
    """Virtual machine lifecycle tests."""

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

    def test_stop(self, instance):
        assert state(f"{instance}") == "Running"
        assert multipass("stop", f"{instance}")
        assert state(f"{instance}") == "Stopped"

    def test_stop_start(self, instance):
        assert state(f"{instance}") == "Running"
        assert multipass("stop", f"{instance}")
        assert state(f"{instance}") == "Stopped"
        assert multipass("start", f"{instance}")
        assert state(f"{instance}") == "Running"

    def test_suspend_resume(self, instance):
        assert state(f"{instance}") == "Running"
        assert multipass("suspend", f"{instance}")
        assert state(f"{instance}") == "Suspended"
        assert multipass("start", f"{instance}")
        assert state(f"{instance}") == "Running"

    @pytest.mark.parametrize(
        "instance",
        [
            {"autopurge": False},
        ],
        indirect=True,
    )
    def test_launch_delete_recover_purge(self, instance):

        assert state(f"{instance}") == "Running"
        assert multipass("delete", f"{instance}")
        assert state(f"{instance}") == "Deleted"

        with multipass("start", f"{instance}") as output:
            assert not output
            assert f"Instance '{instance}' is deleted." in output

        assert multipass("recover", f"{instance}")
        assert state(f"{instance}") == "Stopped"

        assert multipass("start", f"{instance}")

        assert multipass("delete", f"{instance}")
        assert state(f"{instance}") == "Deleted"

        assert multipass("delete", f"{instance}", "--purge")
        with multipass("info", f"{instance}") as output:
            assert not output
            assert "does not exist" in output

    def test_delete_purge(self):
        with launch({"autopurge": False}) as name1, launch(
            {"autopurge": False}
        ) as name2:
            assert multipass("delete", f"{name1}")
            assert state(f"{name1}") == "Deleted"

            assert multipass("delete", f"{name2}")
            assert state(f"{name2}") == "Deleted"

            assert multipass("purge")

            for instance in [name1, name2]:
                with multipass("info", f"{instance}") as output:
                    assert not output
                    assert "does not exist" in output
