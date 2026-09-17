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

"""Unit tests for BooleanLatch synchronization primitive."""

import threading
import time

import pytest

from cli.utilities.threadutils import BooleanLatch


class TestBooleanLatch:
    """Test BooleanLatch thread-safe synchronization."""

    def test_initial_state_is_false(self):
        """BooleanLatch should start with False state."""
        latch = BooleanLatch()
        assert not latch.is_set()

    def test_set_changes_state_to_true(self):
        """set() should change state to True."""
        latch = BooleanLatch()
        latch.set()
        assert latch.is_set()

    def test_clear_changes_state_to_false(self):
        """clear() should change state to False."""
        latch = BooleanLatch()
        latch.set()
        latch.clear()
        assert not latch.is_set()

    def test_wait_until_returns_immediately_when_already_true(self):
        """wait_until(True) should return immediately when already True."""
        latch = BooleanLatch()
        latch.set()

        start = time.monotonic()
        latch.wait_until(True, timeout=1.0)
        elapsed = time.monotonic() - start

        assert elapsed < 0.5

    def test_wait_until_returns_immediately_when_already_false(self):
        """wait_until(False) should return immediately when already False."""
        latch = BooleanLatch()

        start = time.monotonic()
        latch.wait_until(False, timeout=1.0)
        elapsed = time.monotonic() - start

        assert elapsed < 0.5

    def test_wait_until_blocks_until_set(self):
        """wait_until(True) should block until set() is called."""
        latch = BooleanLatch()
        result = []

        def waiter():
            latch.wait_until(True, timeout=2.0)
            result.append(latch.is_set())

        thread = threading.Thread(target=waiter)
        thread.start()

        time.sleep(0.1)
        assert not result

        latch.set()
        thread.join(timeout=1.0)

        assert result == [True]

    def test_wait_until_blocks_until_clear(self):
        """wait_until(False) should block until clear() is called."""
        latch = BooleanLatch()
        latch.set()
        result = []

        def waiter():
            latch.wait_until(False, timeout=2.0)
            result.append(not latch.is_set())

        thread = threading.Thread(target=waiter)
        thread.start()

        time.sleep(0.1)
        assert not result

        latch.clear()
        thread.join(timeout=1.0)

        assert result == [True]

    def test_wait_until_times_out(self):
        """wait_until should return after timeout without raising."""
        latch = BooleanLatch()

        start = time.monotonic()
        latch.wait_until(True, timeout=0.1)
        elapsed = time.monotonic() - start

        assert 0.1 <= elapsed < 1.0
        assert not latch.is_set()

    def test_concurrent_set_and_clear(self):
        """Concurrent set/clear operations should not corrupt state."""
        latch = BooleanLatch()
        iterations = 100

        def setter():
            for _ in range(iterations):
                latch.set()
                time.sleep(0.001)

        def clearer():
            for _ in range(iterations):
                latch.clear()
                time.sleep(0.001)

        threads = [
            threading.Thread(target=setter),
            threading.Thread(target=clearer),
        ]

        for t in threads:
            t.start()

        for t in threads:
            t.join(timeout=2.0)

        assert isinstance(latch.is_set(), bool)

    def test_wait_waits_for_true(self):
        """wait() should wait for True state."""
        latch = BooleanLatch()
        result = []

        def waiter():
            latch.wait()
            result.append(latch.is_set())

        thread = threading.Thread(target=waiter)
        thread.start()

        time.sleep(0.1)
        latch.set()
        thread.join(timeout=1.0)

        assert result == [True]
