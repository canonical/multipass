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

from cli.multipass import (
    multipass,
    multipass_version_has_feature
)

from cli.utilities import(
    is_locale_available
)

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
    ("launch", "Create and start .* instance"),
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


def commands_to_test():
    commands = []
    commands += ALL_COMMANDS

    if multipass_version_has_feature("wait_ready"):
        commands += [
            ("wait-ready", "Wait for the Multipass daemon to be ready")
        ]
    return commands


def available_locales():
    locales = [loc for loc in LOCALES if is_locale_available(loc)]
    if not locales:
        pytest.skip("No test locales available, skipping.")
    return locales


@pytest.mark.help
class TestHelp:
    """Help command tests."""

    def test_help(self):
        failures = []
        for loc in available_locales():
            with multipass(
                "help",
                env={
                    "LC_ALL": loc,
                    "LANG": loc,
                },
            ) as output:
                if not output:
                    failures.append((loc, "help", "non-zero exit code"))
                    continue

                matches = dict(
                    re.findall(
                        r"^ {2}([\-\w]+)\s+(.+?)\r?$",
                        output.content,
                        flags=re.MULTILINE,
                    )
                )

                for cmd, desc_pattern in commands_to_test():
                    if cmd not in matches:
                        failures.append((loc, cmd, "not found in help output"))
                    elif not re.search(desc_pattern, matches[cmd]):
                        failures.append(
                            (loc, cmd, f"description does not match {desc_pattern}")
                        )

        assert not failures, f"help output mismatches: {failures}"

    def test_per_command_help(self):
        failures = []
        for loc in available_locales():
            for cmd, _ in commands_to_test():
                with multipass(
                    "help",
                    cmd,
                    env={
                        "LC_ALL": loc,
                        "LANG": loc,
                    },
                ) as output:
                    if not output:
                        failures.append((loc, cmd))

        assert not failures, f"'help' returned a non-zero exit code for: {failures}"
