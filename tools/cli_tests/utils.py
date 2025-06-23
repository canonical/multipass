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
import contextlib
import jq
import uuid
import time


class JsonOutput(dict):
    def __init__(self, content, returncode):
        self.content = content
        self.returncode = returncode
        try:
            super().__init__(json.loads(content))
        except json.JSONDecodeError:
            super().__init__()

    def jq(self, query):
        """Real jq queries using the jq library"""
        return jq.compile(query).input(dict(self)).all()


class Output:
    def __init__(self, content, returncode):
        self.content = content
        self.returncode = returncode

    def __contains__(self, pattern):
        return re.search(pattern, self.content) is not None

    def __str__(self):
        return self.content


@contextlib.contextmanager
def read_output(proc, timeout=30, retry_until=None):
    try:
        out, _ = proc.communicate(timeout=timeout)

        if out is None:
            out = ""

        if "--format=json" in proc.args:
            yield JsonOutput(out, proc.returncode)
        else:
            yield Output(out, proc.returncode)
    finally:
        if proc.poll() is None:
            proc.kill()

def retry(retries=3, delay=1.0):
    """Decorator to retry a function call based on return code"""
    def decorator(func):
        def wrapper(*args, **kwargs):
            for attempt in range(retries + 1):
                result = func(*args, **kwargs)
                if hasattr(result, 'returncode') and result.returncode == 0:
                    return result
                if attempt < retries:
                    time.sleep(delay)
            return result
        return wrapper
    return decorator



def uuid4_str(prefix="", suffix=""):
    return f"{prefix}{str(uuid.uuid4())}{suffix}"


def is_valid_ipv4_addr(ip_str):
    pattern = r"^((25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.){3}(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$"
    return bool(re.match(pattern, ip_str))
