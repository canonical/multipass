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

import time
from functools import wraps

import pexpect


def retry(retries=3, delay=1.0):
    """Decorator to retry a function call based on return code"""

    def decorator(func):
        def wrapper(*args, **kwargs):
            for attempt in range(retries + 1):
                try:
                    result = func(*args, **kwargs)
                except pexpect.exceptions.TIMEOUT:
                    result = None
                except KeyboardInterrupt:
                    # Do not retry
                    raise
                if hasattr(result, "exitstatus") and result.exitstatus == 0:
                    return result
                if attempt < retries:
                    time.sleep(delay)
            return result

        return wrapper

    return decorator


def wrap_call_if(value, pre=None, post=None, *, index=0, key=None):
    """Run pre() before and post() after fn() if args[index]==value or kwargs[key]==value."""
    def decorator(fn):
        @wraps(fn)
        def wrapper(*args, **kwargs):
            match = (
                (key is not None and kwargs.get(key) == value)
                or (len(args) > index and args[index] == value)
            )
            pre_result = None
            if match and pre:
                pre_result = pre(args, kwargs)
            try:
                result = fn(*args, **kwargs)
            except Exception:
                raise

            if match and post:
                post(args, kwargs, result, pre_result)
            return result
        return wrapper
    return decorator