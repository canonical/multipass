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
