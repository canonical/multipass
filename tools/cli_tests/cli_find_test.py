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

"""Multipass command line tests for the find CLI command."""

import logging

import pytest
import jq

from cli_tests.multipass import (
    multipass,
)

supported_appliances = [
    "appliance:adguard-home",
    "appliance:mosquitto",
    "appliance:nextcloud",
    "appliance:openhab",
    "appliance:plexmediaserver",
]

supported_blueprints = [
    "anbox-cloud-appliance",
    "charm-dev",
    "docker",
    "jellyfin",
    "minikube",
    "ros2-humble",
    "ros2-jazzy",
]


@pytest.mark.find
@pytest.mark.usefixtures("multipassd_class_scoped")
class TestFind:
    """Find command tests."""

    @pytest.mark.parametrize("show", ["", "--only-images", "--only-blueprints"])
    def test_find_all(self, show):
        # Confirm that it shows at least 1 devel, 2 LTS releases
        with multipass("find", "--format=json", show).json() as output:
            assert "blueprints (deprecated)" in output or (show == "--only-images")
            assert "images" in output or (show == "--only-blueprints")
            if show in ["", "--only-blueprints"]:
                # Check blueprints
                blueprints = output.jq('."blueprints (deprecated)" | keys[] ').all()
                assert blueprints == supported_blueprints

            if show in ["", "--only-images"]:
                # Verify that find has at least 2 LTS images
                lts_images = output.jq(
                    '.images | with_entries(select(.value.release | contains("LTS")))'
                ).first()
                assert len(lts_images) >= 2

                # Verify that find has only one image aliased with "lts"
                current_lts = output.jq(
                    '.images | with_entries(select(.value.aliases | index("lts")))'
                ).first()
                assert len(current_lts) == 1
                logging.info(f"Current LTS is {current_lts}")

                # Verify that find has only one image aliased with "devel"
                current_devel = output.jq(
                    '.images | with_entries(select(.value.aliases | index("devel")))'
                ).first()
                assert len(current_devel) == 1
                logging.info(f"Current devel is {current_devel}")

                # Check appliances
                appliances = output.jq(
                    '.images | keys[] | select(startswith("appliance:"))'
                ).all()
                assert appliances == supported_appliances

    @pytest.mark.parametrize(
        "param",
        [
            {"name": "noble", "expected_release": "24.04 LTS"},
            {"name": "24.04", "expected_release": "24.04 LTS"},
            {"name": "20.04", "expected_release": "20.04 LTS", "unsupported": True},
            {"name": "18.04", "expected_release": "18.04 LTS", "unsupported": True},
        ],
    )
    def test_query_image(self, param):
        with multipass(
            "find",
            param["name"],
            "--format=json",
            "--show-unsupported"
            if "unsupported" in param and param["unsupported"]
            else "",
        ).json() as output:
            assert output
            images = output["images"]
            image = images[param["name"]]

            expected_image = {
                "aliases": [],
                "os": "Ubuntu",
                "release": param["expected_release"],
                "remote": "",
            }

            # Pull the keys present in expected and do a comparison
            assert {k: image[k] for k in expected_image} == expected_image
