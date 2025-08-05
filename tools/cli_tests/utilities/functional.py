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

import pexpect
import time


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


def wait_for_future(fut, timeout: float = 60, poll_interval: float = 0.5):
    """
    Wait for a Future to complete without blocking signal handling.

    Args:
        fut: The Future to wait for
        timeout: Maximum time to wait in seconds (default: 60)
        poll_interval: How often to check if done in seconds (default: 0.1)

    Returns:
        The result of the Future

    Raises:
        TimeoutError: If the Future doesn't complete within timeout
        Exception: Whatever exception the Future raised, if any
    """
    start_time = time.time()

    while not fut.done() and (time.time() - start_time) < timeout:
        time.sleep(poll_interval)

    if not fut.done():
        raise TimeoutError(f"Operation timed out after {timeout} seconds")

    if fut.exception():
        raise fut.exception()

    return fut.result()
