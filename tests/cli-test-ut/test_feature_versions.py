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

"""Unit tests for feature-dependent test skipping."""

import pytest
from packaging.version import Version

from cli.multipass import feature_versions

pytest_plugins = ["pytester"]


@pytest.mark.parametrize(
    "feature,version,arch,should_skip",
    [
        ("debian_images", "1.16.0", "x86_64", True),
        ("debian_images", "1.17.0", "s390x", True),
        ("debian_images", "1.17.0", "x86_64", False),
        ("debian_images", "1.17.0", "aarch64", False),
        ("fedora_images", "1.16.0", "s390x", True),
        ("fedora_images", "1.17.0", "s390x", False),
        ("az", "1.16.0", "x86_64", True),
        ("az", "1.17.0", "x86_64", False),
        ("blueprints", "1.16.0", "x86_64", False),
        ("blueprints", "1.17.0", "x86_64", True),
    ],
)
def test_requires_feature_skips_before_fixtures(
    pytester, monkeypatch, feature, version, arch, should_skip
):
    monkeypatch.setattr(
        feature_versions, "get_multipass_version", lambda: Version(version)
    )
    monkeypatch.setattr(feature_versions.platform, "machine", lambda: arch)
    pytester.makepyfile(
        f"""
        import pytest
        from cli.multipass import requires_feature

        @pytest.fixture(autouse=True)
        def daemon():
            assert not {should_skip}, "Daemon fixture ran for an unsupported feature"

        @pytest.fixture
        def instance():
            assert not {should_skip}, "Instance fixture ran for an unsupported feature"
            return "test-instance"

        @requires_feature("{feature}")
        def test_launch(instance):
            assert instance == "test-instance"
        """
    )

    result = pytester.runpytest("-q")

    result.assert_outcomes(passed=int(not should_skip), skipped=int(should_skip))


@pytest.mark.parametrize("version", ["1.16.0", "1.17.0"])
def test_requires_feature_only_skips_marked_parameters(pytester, monkeypatch, version):
    monkeypatch.setattr(
        feature_versions, "get_multipass_version", lambda: Version(version)
    )
    monkeypatch.setattr(feature_versions.platform, "machine", lambda: "x86_64")
    pytester.makepyfile(
        f"""
        import pytest
        from cli.multipass import requires_feature

        @pytest.fixture(autouse=True)
        def daemon(request):
            show = request.node.callspec.params["show"]
            assert not show or "{version}" == "1.16.0"

        @pytest.mark.parametrize("cmd", ["images", "find"])
        @pytest.mark.parametrize("show", [
            "",
            pytest.param("--only-images", marks=requires_feature("blueprints")),
            pytest.param("--only-blueprints", marks=requires_feature("blueprints")),
        ])
        def test_find_all(cmd, show):
            assert cmd in ("images", "find")
        """
    )

    result = pytester.runpytest("-q")

    if version == "1.16.0":
        result.assert_outcomes(passed=6)
    else:
        result.assert_outcomes(passed=2, skipped=4)
