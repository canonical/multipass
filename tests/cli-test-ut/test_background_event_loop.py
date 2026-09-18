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

"""Unit tests for BackgroundEventLoop async task management."""

import asyncio
import time
from unittest import mock

import pytest

from cli.utilities.threadutils import BackgroundEventLoop


class TestBackgroundEventLoop:
    """Test BackgroundEventLoop lifecycle and task draining."""

    @staticmethod
    def _timed_drain_loop(loop, timeout=5.0):
        start = time.monotonic()
        loop.run(loop.drain_loop_until(timeout)).result(timeout=timeout + 1.0)
        return time.monotonic() - start

    def test_context_manager_starts_and_stops(self):
        """Context manager should start and stop the loop."""
        with BackgroundEventLoop() as loop:
            assert loop.loop is not None
            assert loop.thread.is_alive()

        assert loop.loop is None
        assert not loop.thread.is_alive()

    def test_run_returns_future_with_result(self):
        """run() should return a Future that resolves the coroutine result."""

        async def compute():
            await asyncio.sleep(0)
            return 42

        with BackgroundEventLoop() as loop:
            future = loop.run(compute())
            result = future.result(timeout=1.0)

        assert result == 42

    def test_run_fn_schedules_callable(self):
        """run_fn() should schedule a callable on the loop thread."""
        callback = mock.MagicMock()

        with BackgroundEventLoop() as loop:
            loop.run_fn(callback)

        callback.assert_called_once()

    def test_shutdown_is_idempotent(self):
        """Calling shutdown() twice should not crash."""
        loop = BackgroundEventLoop()
        loop.start()
        loop.shutdown()
        loop.shutdown()

    def test_start_is_idempotent(self):
        """Calling start() twice should keep the original loop thread running."""
        loop = BackgroundEventLoop()
        loop.start()
        thread = loop.thread

        try:
            loop.start()

            assert loop.thread is thread
            assert thread.is_alive()
        finally:
            loop.shutdown()

    def test_start_after_shutdown_is_safe(self):
        """Starting and shutting down an already-stopped loop should not crash."""
        loop = BackgroundEventLoop()
        loop.start()
        loop.shutdown()
        loop.start()
        loop.shutdown()

        assert loop.loop is None
        assert not loop.thread.is_alive()

    def test_drain_loop_until_no_tasks(self):
        """drain_loop_until should exit immediately with no pending tasks."""
        with BackgroundEventLoop() as loop:
            elapsed = self._timed_drain_loop(loop, timeout=1.0)

        assert elapsed < 1.0

    def test_drain_loop_until_cancels_pending_tasks(self):
        """drain_loop_until should cancel and await pending tasks."""

        async def hanging_task():
            await asyncio.sleep(3600)

        with BackgroundEventLoop() as loop:
            future = loop.run(hanging_task())
            elapsed = self._timed_drain_loop(loop, timeout=1.0)

            assert future.done()
            assert future.cancelled()
            assert elapsed < 1.5

    def test_drain_loop_until_handles_tasks_spawning_tasks(self):
        """drain_loop_until should handle tasks that spawn new tasks during cancellation."""
        started = asyncio.Event()
        spawned_task = None

        async def spawner():
            nonlocal spawned_task
            started.set()
            try:
                await asyncio.sleep(3600)
            except asyncio.CancelledError:
                spawned_task = asyncio.create_task(asyncio.sleep(3600))
                raise

        with BackgroundEventLoop() as loop:
            future = loop.run(spawner())
            loop.run(started.wait()).result(timeout=1.0)
            self._timed_drain_loop(loop, timeout=1.0)

            assert future.done()
            assert future.cancelled()
            assert spawned_task is not None
            assert spawned_task.done()
            assert spawned_task.cancelled()

    def test_drain_loop_until_times_out_on_resistant_tasks(self):
        """drain_loop_until should timeout on tasks that resist cancellation."""
        started = asyncio.Event()
        cancellation_seen = asyncio.Event()

        async def resistant_task():
            started.set()
            try:
                await asyncio.sleep(3600)
            except asyncio.CancelledError:
                cancellation_seen.set()
                await asyncio.sleep(3600)

        async def run_and_drain():
            task = asyncio.create_task(resistant_task())
            await started.wait()

            start = time.monotonic()
            await loop.drain_loop_until(timeout=0.2)
            elapsed = time.monotonic() - start

            return task, elapsed

        with BackgroundEventLoop() as loop:
            future = loop.run(run_and_drain())
            task, elapsed = future.result(timeout=2.0)

            assert 0.2 <= elapsed < 2.0
            assert cancellation_seen.is_set()
            assert task.done()

    def test_drain_loop_until_handles_multiple_tasks(self):
        """drain_loop_until should cancel multiple pending tasks."""

        async def task1():
            await asyncio.sleep(3600)

        async def task2():
            await asyncio.sleep(3600)

        with BackgroundEventLoop() as loop:
            t1 = loop.run(task1())
            t2 = loop.run(task2())
            self._timed_drain_loop(loop, timeout=1.0)

            assert t1.done() and t1.cancelled()
            assert t2.done() and t2.cancelled()

    def test_drain_loop_until_handles_failing_tasks(self):
        """drain_loop_until should handle tasks that raise exceptions."""
        started = asyncio.Event()

        async def failing_task():
            started.set()
            try:
                await asyncio.sleep(3600)
            except asyncio.CancelledError as exc:
                raise ValueError("task failed") from exc

        with BackgroundEventLoop() as loop:
            future = loop.run(failing_task())
            loop.run(started.wait()).result(timeout=1.0)
            self._timed_drain_loop(loop, timeout=1.0)

            assert future.done()
            with pytest.raises(ValueError, match="task failed"):
                future.result()
