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
    def __init__(self):
        self.loop = asyncio.new_event_loop()
        self.thread = threading.Thread(target=self._run_loop, name="asyncio-loop-thr")
        self.thread.start()

    def _run_loop(self):
        asyncio.set_event_loop(self.loop)
        try:
            self.loop.run_forever()
        finally:
            self.loop.close()

    def run(self, coro):
        """Submit coroutine to background loop"""
        return asyncio.run_coroutine_threadsafe(coro, self.loop)

    def run_fn(self, fn):
        self.loop.call_soon_threadsafe(fn)

    def stop(self):
        self.run_fn(self.loop.stop)
        self.thread.join()
