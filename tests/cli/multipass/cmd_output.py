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

import re

from cli.multipass.cmd_json_output import JsonOutput
from cli.utilities import strip_ansi_escape


def strip_driver_deprecation_notice(text: str) -> str:
    """
    Strip the driver deprecation warning from text.

    The daemon sends it on stderr, which is captured along with stdout, as a
    header line and an advice paragraph, each followed by a blank line.
    """

    return re.sub(
        r"\*\*\* Warning! The \S+ driver is deprecated[^\r\n]*(?:\r*\n){2}.*?(?:\r*\n){2}",
        "",
        text,
        flags=re.S,
    )


class Output:
    """A type to store text command output."""

    def __init__(self, content, exitstatus):
        # Strip ansi escape codes.
        content = strip_ansi_escape(content)
        self.content = strip_driver_deprecation_notice(content).strip()
        self.exitstatus = exitstatus

    def __contains__(self, pattern):
        return re.search(pattern, self.content, flags=re.MULTILINE) is not None

    def __eq__(self, value):
        if isinstance(value, str):
            return self.__str__() == value
        if isinstance(value, bool):
            return self.__bool__() == value
        if isinstance(value, Output):
            return (self.exitstatus == value.exitstatus) and (
                self.content == value.content
            )
        if isinstance(value, dict):
            return self.json() == value
        raise NotImplementedError

    def __str__(self):
        return self.content

    def __enter__(self):
        return self

    def __exit__(self, *args):
        return False

    def __bool__(self):
        return self.exitstatus == 0

    def __repr__(self):
        return f"<output: {self.content} {self.exitstatus}>"

    def json(self):
        """Cast output to JsonOutput."""
        return JsonOutput(self.content, self.exitstatus)
