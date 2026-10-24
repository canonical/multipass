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

import asyncio
import inspect
import time
from functools import wraps

import pexpect


def _succeeded(result):
    """Default ``retry`` success check: truthy, with a zero ``exitstatus`` if present."""
    return bool(result) and getattr(result, "exitstatus", 0) == 0


def retry(retries=3, delay=1.0, *, success_fn=_succeeded):
    """Decorator to retry a function call until it returns a successful result.

    ``success_fn(result)`` decides whether a result is successful; by default a
    result is successful when it is truthy and, if it exposes an ``exitstatus``
    attribute, that exit status is 0. An unsuccessful result is retried, up to
    ``retries`` additional attempts, sleeping ``delay`` seconds in between. The
    last result is returned either way, so callers can inspect it on failure.

    Works on both regular and ``async`` functions; for the latter the wrapper
    is itself a coroutine function and sleeps without blocking the event loop.
    """

    def decorator(func):
        if inspect.iscoroutinefunction(func):

            @wraps(func)
            async def async_wrapper(*args, **kwargs):
                for attempt in range(retries + 1):
                    try:
                        result = await func(*args, **kwargs)
                    except pexpect.exceptions.TIMEOUT:
                        result = None
                    if success_fn(result):
                        return result
                    if attempt < retries:
                        await asyncio.sleep(delay)
                return result

            return async_wrapper

        @wraps(func)
        def wrapper(*args, **kwargs):
            for attempt in range(retries + 1):
                try:
                    result = func(*args, **kwargs)
                except pexpect.exceptions.TIMEOUT:
                    result = None
                if success_fn(result):
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

            if kwargs.pop("disable_hooks", False):
                return fn(*args, **kwargs)

            match = (
                (key is not None and kwargs.get(key) == value)
                or (len(args) > index and args[index] == value)
            )
            pre_result = None
            if match and pre:
                pre_result = pre(args, kwargs)

            result = fn(*args, **kwargs)

            if match and post:
                post(args, kwargs, result, pre_result)
            return result
        return wrapper
    return decorator
