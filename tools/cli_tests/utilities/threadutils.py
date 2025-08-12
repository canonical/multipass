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
import contextlib
from concurrent.futures import Future
from typing import Callable, Optional


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
    def __init__(self, name: str = "asyncio-loop-thr"):
        self.loop = asyncio.new_event_loop()
        self.thread = threading.Thread(target=self._run_loop, name=name, daemon=True)
        self._started = False
        self._stopped = False

    # ---- context manager ----
    def __enter__(self) -> "BackgroundEventLoop":
        self.start()
        return self

    def __exit__(self, exc_type, exc, tb) -> None:
        # self.loop.wait()
        self.shutdown()

    # ---- lifecycle ----
    def start(self) -> None:
        if self._started:
            return
        self._started = True
        self.thread.start()

    def _run_loop(self) -> None:
        asyncio.set_event_loop(self.loop)
        try:
            self.loop.run_forever()
        finally:
            self.loop.close()

    def shutdown(self, cancel_timeout: float = 5.0) -> None:
        if self._stopped:
            return
        self._stopped = True

        # cancel all tasks in the loop, then stop
        async def _graceful_cancel():
            tasks = [t for t in asyncio.all_tasks() if t is not asyncio.current_task()]
            for t in tasks:
                t.cancel()
            if tasks:
                with contextlib.suppress(asyncio.CancelledError):
                    await asyncio.gather(*tasks)

        fut = asyncio.run_coroutine_threadsafe(_graceful_cancel(), self.loop)
        try:
            fut.result(timeout=cancel_timeout)
        except Exception:
            pass  # best-effort; we’re shutting down anyway

        self.loop.call_soon_threadsafe(self.loop.stop)
        self.thread.join()

    # ---- submission helpers ----
    def run(self, coro) -> Future:
        """Submit a coroutine to the background loop; returns concurrent.futures.Future."""
        if self._stopped:
            raise RuntimeError("BackgroundEventLoop is stopped")
        if not self._started:
            self.start()
        return asyncio.run_coroutine_threadsafe(coro, self.loop)

    def run_fn(self, fn: Callable[[], None]) -> None:
        """Schedule a plain callable on the loop thread."""
        if self._stopped:
            raise RuntimeError("BackgroundEventLoop is stopped")
        if not self._started:
            self.start()
        self.loop.call_soon_threadsafe(fn)
