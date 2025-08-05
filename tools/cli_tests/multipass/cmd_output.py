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

from .cmd_json_output import JsonOutput


class Output:
    """A type to store text command output."""

    def __init__(self, content, exitstatus):
        self.content = content.strip()
        self.exitstatus = exitstatus

    def __contains__(self, pattern):
        return re.search(pattern, self.content) is not None

    def __eq__(self, value):
        if isinstance(value, str):
            return self.__str__() == value
        if isinstance(value, bool):
            return self.__bool__() == value
        if isinstance(value, Output):
            return (self.exitstatus == value.exitstatus) and (
                self.content == value.content
            )
        raise NotImplementedError

    def __str__(self):
        return self.content

    def __enter__(self):
        return self

    def __exit__(self, *args):
        return False

    def __bool__(self):
        return self.exitstatus == 0

    def json(self):
        """Cast output to JsonOutput."""
        return JsonOutput(self.content, self.exitstatus)
