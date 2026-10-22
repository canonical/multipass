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

from cli.multipass import multipass


@pytest.mark.alias
@pytest.mark.usefixtures("multipassd_class_scoped")
class TestAlias:
    """Alias command tests."""

    @pytest.mark.parametrize(
        "instance",
        [
            {"assert": {"delete": False}},
        ],
        indirect=True,
    )
    def test_create_alias(self, instance):
        """Create an alias in the default context.
        Then verify that;
        - the alias works as expected
        - alias is listed correctly
        - alias is removed correctly
        """
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
            "context-status": {"default": "active"},
        }
        # Delete the instance and verify that the alias is also removed
        assert multipass("delete", instance, "--force")
        assert not multipass("wai")
        assert multipass("aliases", "--format=json").json() == {
            "active-context": "default",
            "contexts": {"default": {}},
            "context-status": {"default": "active"},
        }

    def test_prefer_does_not_switch_context(self, instance):
        """Rejected context switches leave aliases usable in default."""
        assert not multipass("prefer", "foo")
        assert multipass("alias", f"{instance}:whoami", "wai")
        assert not multipass("prefer", "bar")
        assert multipass(
            "alias", f"{instance}:sudo", "si", "--no-map-working-directory"
        )
        assert multipass("wai") == "ubuntu"
        assert multipass("default.wai") == "ubuntu"
        assert multipass("si", "whoami") == "root"
        assert multipass("default.si", "whoami") == "root"
        assert multipass("aliases", "--format=json").json() == {
            "active-context": "default",
            "contexts": {
                "default": {
                    "si": {
                        "command": "sudo",
                        "instance": instance,
                        "working-directory": "default",
                    },
                    "wai": {
                        "command": "whoami",
                        "instance": instance,
                        "working-directory": "map",
                    }
                },
            },
            "context-status": {"default": "active"},
        }
        assert multipass("unalias", "si")
        assert multipass("unalias", "default.wai")
        assert multipass("aliases", "--format=json").json() == {
            "active-context": "default",
            "contexts": {
                "default": {},
            },
            "context-status": {"default": "active"},
        }
