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

import threading
import asyncio
import time
from concurrent.futures import Future
from typing import Callable
from contextlib import suppress


class BooleanLatch:
    def __init__(self):
        self._flag = False
        self._cond = threading.Condition()

    def is_set(self):
        with self._cond:
            return self._flag

    def set(self):
        with self._cond:
            self._flag = True
            self._cond.notify_all()

    def clear(self):
        with self._cond:
            self._flag = False
            self._cond.notify_all()

    def wait_until(self, value: bool, timeout=None):
        with self._cond:
            self._cond.wait_for(lambda: self._flag == value, timeout=timeout)

    def wait(self):
        self.wait_until(True)


class BackgroundEventLoop:
    def __init__(self, name: str = "background-event-loop"):
        self.loop = asyncio.new_event_loop()
        # self.loop.set_debug(True)
        self.thread = threading.Thread(target=self._run_loop, name=name, daemon=True)
        self._started = False
        self._stopped = False

    # ---- context manager ----
    def __enter__(self) -> "BackgroundEventLoop":
        self.start()
        return self

    def __exit__(self, exc_type, exc, tb) -> None:
        self.shutdown()

    # ---- lifecycle ----
    def start(self) -> None:
        if self._started:
            return
        self._started = True
        self.thread.start()

    def _run_loop(self) -> None:
        self.loop.run_forever()

    def shutdown(self, cancel_timeout: float = 5.0) -> None:
        if self._stopped:
            return
        self._stopped = True

        # Poll the loop for 30s for any upcoming scheduled tasks.
        self.run(self.drain_loop_until(30)).result()
        # Declare the loop officially closed.
        self.loop.call_soon_threadsafe(self.loop.stop)
        # Join the worker thread.
        self.thread.join()
        # Close the loop
        self.loop.close()
        self.loop = None

    # ---- submission helpers ----
    def run(self, coro) -> Future:
        """Submit a coroutine to the background loop; returns concurrent.futures.Future."""
        try:
            return asyncio.run_coroutine_threadsafe(coro, self.loop)
        except Exception:
            with suppress(Exception):
                coro.close()
            raise

    def run_fn(self, fn: Callable[[], None]) -> None:
        """Schedule a plain callable on the loop thread."""

        self.loop.call_soon_threadsafe(fn)

    async def drain_loop_until(self, timeout: float = 5.0):
        """Cancel and await tasks until none remain or timeout reached.
        Ensure a graceful shutdown for all the tasks scheduled.
        """

        deadline = time.monotonic() + timeout
        seen: set[asyncio.Task] = set()

        while True:
            # All tasks except *this* one
            current = {
                t
                for t in asyncio.all_tasks(self.loop)
                if t is not asyncio.current_task()
            }
            # Tasks we haven't handled yet
            new = [t for t in current if t not in seen]

            if not new:
                break  # No new tasks left

            # Cancel the newcomers
            for t in new:
                if not t.done():
                    t.cancel()

            # Await their completion, bounded by remaining time
            try:
                await asyncio.wait_for(
                    asyncio.gather(*new, return_exceptions=True),
                    timeout=max(0.0, deadline - time.monotonic()),
                )
            except asyncio.TimeoutError:
                # Still stuck tasks — optional debug output
                for t in new:
                    if not t.done():
                        t.print_stack()
                break

            seen.update(new)

            # Stop if we've hit the deadline
            if time.monotonic() >= deadline:
                break


class AsyncSubprocess:
    """Context manager for subprocess with guaranteed cleanup."""

    def __init__(self, *args, **kwargs):
        self.args = args
        self.kwargs = kwargs
        self.proc = None

    async def __aenter__(self):
        # Shield the spawn so it completes even if we get cancelled mid-await.
        fut = asyncio.shield(asyncio.create_subprocess_exec(*self.args, **self.kwargs))
        try:
            self.proc = await fut
            return self.proc
        except asyncio.CancelledError:
            # If cancellation hit mid-spawn, the process may already exist.
            # Finish the spawn to obtain the handle, clean it up, then re-raise.
            try:
                self.proc = await fut
            except Exception:
                # Spawn actually failed; nothing to clean.
                raise
            # Drain & close deterministically
            if self.proc.returncode is None:
                with suppress(ProcessLookupError):
                    self.proc.terminate()
                try:
                    await asyncio.wait_for(self.proc.communicate(), 5)
                except asyncio.TimeoutError:
                    with suppress(ProcessLookupError):
                        self.proc.kill()
                    await self.proc.communicate()
            raise

    async def __aexit__(self, exc_type, exc_val, exc_tb):
        p = self.proc
        self.proc = None

        try:
            if p.returncode is None:
                # Try to end things gracefully
                with suppress(ProcessLookupError):
                    p.terminate()
                try:
                    await asyncio.wait_for(p.communicate(), 5)
                except TimeoutError:
                    with suppress(ProcessLookupError):
                        p.kill()

                await p.communicate()
        except asyncio.CancelledError:
            # Ensure the subprocess is cleaned after.
            await p.communicate()
            # We did the cleanup; now propagate cancel.
            raise
        # return False -> propagate any exception from the 'with' body
        return False


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
