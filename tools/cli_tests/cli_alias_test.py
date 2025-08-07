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

"""Multipass command line tests for the alias CLI command."""

import pytest

from cli_tests.utilities import uuid4_str
from cli_tests.multipass import (
    multipass,
)


@pytest.mark.alias
@pytest.mark.usefixtures("multipassd_class_scoped")
class TestAlias:
    """Alias command tests."""

    @pytest.mark.parametrize(
        "instance",
        [
            {"assert": {"purge": False}},
        ],
        indirect=True,
    )
    def test_create_alias(self, instance):
        """Try to list instances whilst there are none."""
        assert multipass("prefer", "default")
        assert multipass("alias", f"{instance}:whoami", "wai")
        assert multipass("wai") == "ubuntu"
        assert multipass("default.wai") == "ubuntu"
        assert multipass("aliases", "--format=json").json() == {
            "active-context": "default",
            "contexts": {
                "default": {
                    "wai": {
                        "command": "whoami",
                        "instance": instance,
                        "working-directory": "map",
                    }
                }
            },
        }
        # Purge the instance and verify that the alias is also removed
        assert multipass("delete", instance, "--purge")
        assert not multipass("wai")
        assert multipass("aliases", "--format=json").json() == {
            "active-context": "default",
            "contexts": {"default": {}},
        }

    def test_create_alias_in_another_context(self, instance):
        """Try to list instances whilst there are none."""
        assert multipass("prefer", "foo")
        assert multipass("alias", f"{instance}:whoami", "wai")
        assert multipass("prefer", "bar")
        assert multipass(
            "alias", f"{instance}:sudo", "si", "--no-map-working-directory"
        )
        assert multipass("prefer", "foo")
        assert multipass("wai") == "ubuntu"
        assert multipass("foo.wai") == "ubuntu"
        assert multipass("prefer", "bar")
        assert multipass("si", "whoami") == "root"
        assert multipass("bar.si", "whoami") == "root"
        assert multipass("aliases", "--format=json").json() == {
            "active-context": "bar",
            "contexts": {
                "bar": {
                    "si": {
                        "command": "sudo",
                        "instance": instance,
                        "working-directory": "default",
                    }
                },
                "foo": {
                    "wai": {
                        "command": "whoami",
                        "instance": instance,
                        "working-directory": "map",
                    }
                },
            },
        }
        assert multipass("unalias", "si")
        assert multipass("unalias", "foo.wai")
        assert multipass("aliases", "--format=json").json() == {
            "active-context": "bar",
            "contexts": {
                "bar": {},
            },
        }
