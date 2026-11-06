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

"""Unit tests for Multipass version parsing helpers."""

from contextlib import contextmanager
from unittest.mock import MagicMock, patch

import pytest
from packaging.version import Version

from cli.multipass.helpers import get_multipass_version, parse_multipass_versions


@pytest.mark.parametrize(
    "output,expected",
    [
        ("multipass  1.16.0\nmultipassd  1.16.0\n", ("1.16.0", "1.16.0")),
        (
            (
                "multipass   1.17.0-dev.10+g12345678.win\n"
                "multipassd  1.17.0-dev.11+g87654321.win\n"
            ),
            ("1.17.0-dev.10+g12345678.win", "1.17.0-dev.11+g87654321.win"),
        ),
        ("multipass  1.17.0+mac\n", ("1.17.0+mac", None)),
    ],
)
def test_parse_multipass_versions_keeps_full_version(output, expected):
    assert parse_multipass_versions(output) == expected


@pytest.mark.parametrize(
    "output",
    [
        "multipassd  1.16.0\n",
        "multipass  1.16.0\nmultipassd  unknown\n",
    ],
)
def test_parse_multipass_versions_rejects_malformed_output(output):
    with pytest.raises(AssertionError, match="Could not extract"):
        parse_multipass_versions(output)


@pytest.mark.parametrize(
    "cli_version",
    ["1.17.0", "1.17.0-dev.10+g12345678.win", "1.17.0+mac"],
)
def test_get_multipass_version_returns_release_version(cli_version):
    @contextmanager
    def fake_multipass(*args, **kwargs):
        yield MagicMock(
            content=f"multipass  {cli_version}\nmultipassd  {cli_version}\n"
        )

    get_multipass_version.cache_clear()
    try:
        with patch("cli.multipass.helpers.multipass", side_effect=fake_multipass):
            assert get_multipass_version() == Version("1.17.0")
    finally:
        get_multipass_version.cache_clear()
