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

"""Multipass command line tests for the help CLI command."""

import re

import pytest

from cli_tests.multipass import (
    multipass,
)
from cli_tests.utilities import run_as_subprocess

LOCALES = [
    "C",
    "C.UTF-8",
    "en_US.UTF-8",
    "tr_TR.UTF-8",
    "de_DE.UTF-8",
    "fr_FR.UTF-8",
    # Windows-friendly fallbacks (won't exist on Linux, we skip if missing):
    "en-US",
    "tr-TR",
]


ALL_COMMANDS = [
    ("alias", "Create an alias"),
    ("aliases", "List available aliases"),
    ("authenticate", "Authenticate client"),
    ("clone", "Clone an instance"),
    ("delete", "Delete instances and snapshots"),
    ("exec", "Run a command on an instance"),
    ("find", "Display available images to create instances from"),
    ("get", "Get a configuration setting"),
    ("help", "Display help about a command"),
    ("info", "Display information about instances or snapshots"),
    ("launch", "Create and start an Ubuntu instance"),
    ("list", "List all available instances or snapshots"),
    ("mount", "Mount a local directory in the instance"),
    ("networks", "List available network interfaces"),
    ("prefer", "Switch the current alias context"),
    ("purge", "Purge all deleted instances permanently"),
    ("recover", "Recover deleted instances"),
    ("restart", "Restart instances"),
    ("restore", "Restore an instance from a snapshot"),
    ("set", "Set a configuration setting"),
    ("shell", "Open a shell on an instance"),
    ("snapshot", "Take a snapshot of an instance"),
    ("start", "Start instances"),
    ("stop", "Stop running instances"),
    ("suspend", "Suspend running instances"),
    ("transfer", "Transfer files between the host and instances"),
    ("umount", "Unmount a directory from an instance"),
    ("unalias", "Remove aliases"),
    ("version", "Show version details"),
]


def set_locale(loc):
    import locale

    locale.setlocale(locale.LC_ALL, loc)


def locale_available(loc):
    """Intended to be run in a subprocess."""
    import subprocess

    return (
        run_as_subprocess(
            set_locale,
            loc,
            check=False,
            stdout=None,
            stderr=None,
        ).returncode
        == 0
    )


@pytest.mark.help
@pytest.mark.parametrize("loc", LOCALES)
# No need for daemon.
# @pytest.mark.usefixtures("multipassd_class_scoped")
class TestHelp:
    """Alias command tests."""

    def test_help(self, loc):
        if not locale_available(loc):
            pytest.skip(f"Locale {loc} not available, skipping.")

        with multipass(
            "help",
            env={
                "LC_ALL": loc,
                "LANG": loc,
            },
        ) as output:
            assert output

            matches = re.findall(
                r"^ {2}(\w+)\s+(.*)\r$", output.content, flags=re.MULTILINE
            )

            assert ALL_COMMANDS == matches

    @pytest.mark.parametrize("cmd", (x[0] for x in ALL_COMMANDS))
    def test_per_command_help(self, loc, cmd):
        if not locale_available(loc):
            pytest.skip(f"Locale {loc} not available, skipping.")

        with multipass(
            "help",
            cmd,
            env={
                "LC_ALL": loc,
                "LANG": loc,
            },
        ) as output:
            assert output
