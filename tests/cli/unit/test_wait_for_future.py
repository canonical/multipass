#!/usr/bin/env python3
"""Unit tests for wait_for_future utility function."""

import asyncio
import time
from concurrent.futures import Future

import pytest

from cli.utilities.threadutils import wait_for_future


class TestWaitForFuture:
    """Test wait_for_future polling-based future waiter."""

    def test_returns_result_when_future_completes(self):
        """wait_for_future should return result when future completes."""
        future = Future()
        future.set_result(42)

        result = wait_for_future(future, timeout=1.0)

        assert result == 42

    def test_raises_timeout_error_when_future_doesnt_complete(self):
        """wait_for_future should raise TimeoutError when future doesn't complete."""
        future = Future()

        with pytest.raises(TimeoutError, match="Operation timed out"):
            wait_for_future(future, timeout=0.1, poll_interval=0.01)

    def test_propagates_exception_from_future(self):
        """wait_for_future should propagate exception raised by the future."""
        future = Future()
        future.set_exception(ValueError("test error"))

        with pytest.raises(ValueError, match="test error"):
            wait_for_future(future, timeout=1.0)

    def test_handles_already_completed_future(self):
        """wait_for_future should handle already-completed future."""
        future = Future()
        future.set_result("done")

        result = wait_for_future(future, timeout=1.0)

        assert result == "done"

    def test_waits_for_delayed_completion(self):
        """wait_for_future should wait for future that completes after delay."""
        import threading

        future = Future()

        def delayed_set():
            time.sleep(0.05)
            future.set_result("delayed")

        thread = threading.Thread(target=delayed_set)
        thread.start()

        result = wait_for_future(future, timeout=1.0, poll_interval=0.01)

        thread.join()
        assert result == "delayed"

    def test_cancels_future_on_timeout(self):
        """wait_for_future should cancel future on timeout."""
        future = Future()

        with pytest.raises(TimeoutError):
            wait_for_future(future, timeout=0.1, poll_interval=0.01)

        assert future.cancelled()

    def test_respects_custom_poll_interval(self):
        """wait_for_future should respect custom poll interval."""
        future = Future()
        future.set_result(42)

        result = wait_for_future(future, timeout=1.0, poll_interval=0.5)

        assert result == 42
