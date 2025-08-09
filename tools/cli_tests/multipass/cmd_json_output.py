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

import json

import jq


class JsonOutput(dict):
    """A type to store JSON command output."""

    def __init__(self, content, exitstatus):
        self.content = content.strip()
        self.exitstatus = exitstatus
        try:
            super().__init__(json.loads(content))
        except json.JSONDecodeError:
            super().__init__()

    def __enter__(self):
        return self

    def __exit__(self, *args):
        return False

    def __bool__(self):
        return self.exitstatus == 0

    def jq(self, query):
        """Real jq queries using the jq library"""
        return jq.compile(query).input(dict(self)).all()
