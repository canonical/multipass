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

import re
import uuid


def strip_ansi_escape(input):
    # https://stackoverflow.com/a/14693789/10872971
    ansi_escape_8bit = re.compile(
        r"""
        (?: # either 7-bit C1, two bytes, ESC Fe (omitting CSI)
            \x1B
            [@-Z\\-_]
        |   # or a single 8-bit byte Fe (omitting CSI)
            [\x80-\x9A\x9C-\x9F]
        |   # or CSI + control codes
            (?: # 7-bit CSI, ESC [
                \x1B\[
            |   # 8-bit CSI, 9B
                \x9B
            )
            [0-?]*  # Parameter bytes
            [ -/]*  # Intermediate bytes
            [@-~]   # Final byte
        )
    """,
        re.VERBOSE,
    )
    return ansi_escape_8bit.sub("", input)


def uuid4_str(prefix="", suffix=""):
    """Generate an UUID4 string, prefixed/suffixed with the given params."""
    return f"{prefix}{str(uuid.uuid4())}{suffix}"


def is_valid_ipv4_addr(ip_str):
    """Validate that given ip_str is a valid IPv4 address."""
    pattern = r"^((25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.){3}(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$"
    return bool(re.match(pattern, ip_str))
