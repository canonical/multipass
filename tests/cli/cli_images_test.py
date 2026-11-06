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

"""Multipass command line tests for the images CLI command."""

import logging

import pytest

from cli.multipass import (
    multipass,
    multipass_version_has_feature,
    skip_if_feature_not_supported,
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


@pytest.mark.images
@pytest.mark.usefixtures("multipassd_class_scoped")
class TestImages:
    """Images command tests."""

    @pytest.fixture(params=["images", "find"])
    def cmd(self, request):
        # Run against both "images" and deprecated "find" alias.
        return request.param

    @pytest.mark.parametrize(
        "show",
        ["", "--only-images", "--only-blueprints"],
    )
    def test_find_all(self, cmd, show):
        if show in ["--only-images", "--only-blueprints"]:
            skip_if_feature_not_supported("blueprints")

        # Confirm that it shows at least 1 devel, 2 LTS releases
        (output, header) = multipass(
            cmd, "--all", "--format=json", show
        ).json_with_header()
        assert ("Warning:" in header) == (cmd == "find")
        assert "images" in output or show == "--only-blueprints"

        if multipass_version_has_feature("blueprints"):
            if show in ["", "--only-blueprints"]:
                assert "blueprints (deprecated)" in output or (
                    show == "--only-images"
                )
                # Check blueprints
                blueprints = output.jq(
                    '."blueprints (deprecated)" | keys[]'
                ).all()
                assert blueprints == supported_blueprints

        if show in ["", "--only-images"]:
            assert "images" in output
            # Verify that find has at least 2 LTS images
            lts_images = output.jq(
                '.images | with_entries(select(.value.remote == "release" and (.value.release | contains("LTS"))))'
            ).first()
            assert len(lts_images) >= 2

            # Verify that find has only one image aliased with "lts"
            current_lts = output.jq(
                '.images | with_entries(select(.value.remote == "release" and (.value.aliases | index("lts"))))'
            ).first()
            assert len(current_lts) == 1
            logging.debug(f"Current LTS is {current_lts}")

            # Verify that find has only one image aliased with "devel"
            current_devel = output.jq(
                '.images | with_entries(select(.value.remote == "daily" and (.value.aliases | index("devel"))))'
            ).first()
            assert len(current_devel) == 1
            logging.debug(f"Current devel is {current_devel}")

            if multipass_version_has_feature("appliances"):
                # Check appliances
                appliances = output.jq(
                    '.images | keys[] | select(startswith("appliance:"))'
                ).all()
                assert appliances == supported_appliances

    @pytest.mark.parametrize(
        ("query", "image", "aliases", "expected_release", "show_unsupported"),
        [
            ("noble", "24.04", ["noble"], "24.04 LTS", False),
            ("24.04", "24.04", ["noble"], "24.04 LTS", False),
            ("20.04", "20.04", ["focal"], "20.04 LTS", True),
            ("18.04", "18.04", ["bionic"], "18.04 LTS", True),
        ],
    )
    def test_query_image_ubuntu(
        self,
        cmd,
        query,
        image,
        aliases,
        expected_release,
        show_unsupported,
    ):
        (output, header) = multipass(
            cmd,
            query,
            "--format=json",
            *(["--show-unsupported"] if show_unsupported else []),
        ).json_with_header()

        assert ("Warning:" in header) == (cmd == "find")

        assert output
        images = output["images"]
        image_data = images[image]

        expected_image = {
            "aliases": aliases,
            "os": "Ubuntu",
            "release": expected_release,
            "remote": "release",
        }

        # Pull the keys present in expected and do a comparison
        assert {key: image_data[key] for key in expected_image} == expected_image

    def test_query_image_debian(self, cmd):
        skip_if_feature_not_supported("debian_images")

        (output, header) = multipass(
            cmd, "debian", "--format=json"
        ).json_with_header()
        assert ("Warning:" in header) == (cmd == "find")

        assert output
        image = output["images"]["debian"]

        expected_image = {
            "aliases": ["trixie"],
            "os": "Debian",
            "remote": "",
        }

        assert {key: image[key] for key in expected_image} == expected_image
        assert image["release"]

    def test_query_image_fedora(self, cmd):
        skip_if_feature_not_supported("fedora_images")

        (output, header) = multipass(
            cmd, "fedora", "--format=json"
        ).json_with_header()
        assert ("Warning:" in header) == (cmd == "find")

        assert output
        image = output["images"]["fedora"]

        expected_image = {
            "aliases": [],
            "os": "Fedora",
            "remote": "",
        }

        assert {key: image[key] for key in expected_image} == expected_image
        assert image["release"]

    def test_query_remotes(self, cmd):
        (output, header) = multipass(
            cmd, "--remotes", "--format=json"
        ).json_with_header()
        assert ("Warning:" in header) == (cmd == "find")

        assert output
        remotes = output["remotes"]
        assert remotes == ["core", "daily", "release", "snapcraft"]
