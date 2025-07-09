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
import json
import uuid
import time

import pexpect
import jq


class JsonOutput(dict):
    """A type to store JSON command output."""

    def __init__(self, content, exitstatus):
        self.content = content
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


class Output:
    """A type to store text command output."""

    def __init__(self, content, exitstatus):
        self.content = content
        self.exitstatus = exitstatus

    def __contains__(self, pattern):
        return re.search(pattern, self.content) is not None

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


def retry(retries=3, delay=1.0):
    """Decorator to retry a function call based on return code"""

    def decorator(func):
        def wrapper(*args, **kwargs):
            for attempt in range(retries + 1):
                try:
                    result = func(*args, **kwargs)
                except pexpect.exceptions.TIMEOUT:
                    result = None
                if hasattr(result, "exitstatus") and result.exitstatus == 0:
                    return result
                if attempt < retries:
                    time.sleep(delay)
            return result

        return wrapper

    return decorator


def uuid4_str(prefix="", suffix=""):
    """Generate an UUID4 string, prefixed/suffixed with the given params."""
    return f"{prefix}{str(uuid.uuid4())}{suffix}"


def is_valid_ipv4_addr(ip_str):
    """Validate that given ip_str is a valid IPv4 address."""
    pattern = r"^((25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.){3}(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$"
    return bool(re.match(pattern, ip_str))
