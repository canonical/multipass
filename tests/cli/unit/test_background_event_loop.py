#!/usr/bin/env python3
"""Unit tests for BackgroundEventLoop async task management."""

import asyncio
import time

import pytest

from cli.utilities.threadutils import BackgroundEventLoop


class TestBackgroundEventLoop:
    """Test BackgroundEventLoop lifecycle and task draining."""

    def test_context_manager_starts_and_stops(self):
        """Context manager should start and stop the loop."""
        with BackgroundEventLoop() as loop:
            assert loop.loop is not None
            assert loop.thread.is_alive()

        assert not loop.thread.is_alive()

    def test_run_returns_future_with_result(self):
        """run() should return a Future that resolves the coroutine result."""

        async def compute():
            await asyncio.sleep(0.01)
            return 42

        with BackgroundEventLoop() as loop:
            future = loop.run(compute())
            result = future.result(timeout=1.0)

        assert result == 42

    def test_run_fn_schedules_callable(self):
        """run_fn() should schedule a callable on the loop thread."""
        results = []
        import threading

        called = threading.Event()

        def callback():
            results.append("called")
            called.set()

        with BackgroundEventLoop() as loop:
            loop.run_fn(callback)
            assert called.wait(timeout=1.0)

        assert results == ["called"]

    def test_shutdown_is_idempotent(self):
        """Calling shutdown() twice should not crash."""
        loop = BackgroundEventLoop()
        loop.start()
        loop.shutdown()
        loop.shutdown()

    def test_drain_loop_until_no_tasks(self):
        """drain_loop_until should exit immediately with no pending tasks."""

        async def check_drain():
            start = time.monotonic()
            await loop.drain_loop_until(timeout=1.0)
            return time.monotonic() - start

        with BackgroundEventLoop() as loop:
            future = loop.run(check_drain())
            elapsed = future.result(timeout=2.0)

        assert elapsed < 1.0

    def test_drain_loop_until_cancels_pending_tasks(self):
        """drain_loop_until should cancel and await pending tasks."""

        async def hanging_task():
            await asyncio.sleep(3600)

        async def run_and_drain():
            task = asyncio.create_task(hanging_task())

            start = time.monotonic()
            await loop.drain_loop_until(timeout=1.0)
            elapsed = time.monotonic() - start

            return task, elapsed

        with BackgroundEventLoop() as loop:
            future = loop.run(run_and_drain())
            task, elapsed = future.result(timeout=2.0)

            assert task.done()
            assert task.cancelled()
            assert elapsed < 1.5

    def test_drain_loop_until_handles_tasks_spawning_tasks(self):
        """drain_loop_until should handle tasks that spawn new tasks during cancellation."""
        spawned = []

        async def spawner():
            try:
                await asyncio.sleep(3600)
            except asyncio.CancelledError:
                spawned.append(asyncio.create_task(asyncio.sleep(0.01)))
                raise

        async def run_and_drain():
            task = asyncio.create_task(spawner())
            await loop.drain_loop_until(timeout=1.0)
            return task

        with BackgroundEventLoop() as loop:
            future = loop.run(run_and_drain())
            task = future.result(timeout=2.0)

            assert task.done()
            for t in spawned:
                assert t.done()

    def test_drain_loop_until_times_out_on_resistant_tasks(self):
        """drain_loop_until should timeout on tasks that resist cancellation."""

        async def resistant_task():
            try:
                await asyncio.sleep(3600)
            except asyncio.CancelledError:
                await asyncio.sleep(3600)

        async def run_and_drain():
            task = asyncio.create_task(resistant_task())
            await asyncio.sleep(0.01)  # Let task start and become visible

            start = time.monotonic()
            await loop.drain_loop_until(timeout=0.2)
            elapsed = time.monotonic() - start

            return task, elapsed

        with BackgroundEventLoop() as loop:
            future = loop.run(run_and_drain())
            task, elapsed = future.result(timeout=2.0)

            assert 0.2 <= elapsed < 2.0

    def test_drain_loop_until_handles_multiple_tasks(self):
        """drain_loop_until should cancel multiple pending tasks."""

        async def task1():
            await asyncio.sleep(3600)

        async def task2():
            await asyncio.sleep(3600)

        async def run_and_drain():
            t1 = asyncio.create_task(task1())
            t2 = asyncio.create_task(task2())

            await loop.drain_loop_until(timeout=1.0)

            return t1, t2

        with BackgroundEventLoop() as loop:
            future = loop.run(run_and_drain())
            t1, t2 = future.result(timeout=2.0)

            assert t1.done() and t1.cancelled()
            assert t2.done() and t2.cancelled()

    def test_drain_loop_until_handles_failing_tasks(self):
        """drain_loop_until should handle tasks that raise exceptions."""

        async def failing_task():
            await asyncio.sleep(0.01)
            raise ValueError("task failed")

        async def run_and_drain():
            task = asyncio.create_task(failing_task())
            await asyncio.sleep(0.05)

            await loop.drain_loop_until(timeout=1.0)

            return task

        with BackgroundEventLoop() as loop:
            future = loop.run(run_and_drain())
            task = future.result(timeout=2.0)

            assert task.done()
            assert isinstance(task.exception(), ValueError)
