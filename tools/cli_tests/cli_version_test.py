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

"""Multipass command line tests for the version CLI command."""

import re

import pytest

from cli_tests.multipass import (
    multipass,
)


@pytest.mark.version
class TestVersion:
    """Version command tests."""

    # https://semver.org/#is-there-a-suggested-regular-expression-regex-to-check-a-semver-string
    version_format_regex = r"^(\w+)\s+(?P<major>0|[1-9]\d*)\.(?P<minor>0|[1-9]\d*)\.(?P<patch>0|[1-9]\d*)(?:-(?P<prerelease>(?:0|[1-9]\d*|\d*[a-zA-Z-][0-9a-zA-Z-]*)(?:\.(?:0|[1-9]\d*|\d*[a-zA-Z-][0-9a-zA-Z-]*))*))?(?:\+(?P<buildmetadata>[0-9a-zA-Z-]+(?:\.[0-9a-zA-Z-]+)*))?\r?$"

    def test_version_no_daemon(self):
        with multipass("version") as output:
            assert output
            matches = re.findall(
                self.version_format_regex, output.content, flags=re.MULTILINE
            )

            assert len(matches) == 1
            assert matches[0][0] == "multipass"

    def test_version_with_daemon(self, multipassd):
        with multipass("version") as output:
            assert output

            matches = re.findall(
                self.version_format_regex, output.content, flags=re.MULTILINE
            )

            assert len(matches) == 2
            assert matches[0][0] == "multipass"
            assert matches[1][0] == "multipassd"
            # Verify that the daemon and the client versions match.
            assert matches[0][1:] == matches[1][1:]
