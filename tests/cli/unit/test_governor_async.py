#!/usr/bin/env python3
"""Unit tests for MultipassdGovernor async paths."""

import asyncio
from contextlib import ExitStack
from unittest.mock import AsyncMock, MagicMock, patch

import pytest

from cli.controller.multipassd_governor import MultipassdGovernor
from cli.multipass.exceptions import TestCaseFailure, TestSessionFailure


class MockController:
    """Mock controller for testing governor async behavior."""

    def __init__(self, exit_code=None, exit_delay=0.0):
        self.exit_code_value = exit_code
        self.exit_delay = exit_delay
        self.start_called = False
        self.stop_called = False
        self.supports_autorestart = False
        self._stop_event = asyncio.Event()

    async def start(self):
        self.start_called = True
        self._stop_event.clear()

    async def stop(self, graceful=True):
        self.stop_called = True
        self._stop_event.set()

    async def restart(self):
        await self.stop()
        await self.start()

    async def follow_output(self):
        return
        yield

    async def is_active(self):
        return self.exit_code_value is None and not self._stop_event.is_set()

    async def wait_exit(self):
        if self.exit_code_value is None:
            await self._stop_event.wait()
            return 0
        if self.exit_delay > 0:
            await asyncio.sleep(self.exit_delay)
        return self.exit_code_value

    async def exit_code(self):
        return self.exit_code_value

    def supports_self_autorestart(self):
        return self.supports_autorestart

    async def wait_for_self_autorestart(self):
        pass


@pytest.fixture
def controller():
    """Factory fixture for creating MockController instances."""
    def _factory(exit_code=None, exit_delay=0.0):
        return MockController(exit_code=exit_code, exit_delay=exit_delay)
    return _factory


async def run_governor(controller, ready_fn=None, exit_fn=None):
    """Start governor with mocked dependencies."""
    async def hang_forever():
        await asyncio.sleep(3600)

    async def noop(*args, **kwargs):
        pass

    governor = MultipassdGovernor(controller, None, print_daemon_output=False)
    with ExitStack() as stack:
        stack.enter_context(patch.object(governor, '_ensure_client_certs_are_created', new_callable=AsyncMock))
        stack.enter_context(patch.object(governor, '_authenticate_client_cert'))
        stack.enter_context(patch.object(governor, 'wait_for_multipassd_ready', side_effect=ready_fn or hang_forever))
        stack.enter_context(patch.object(governor, 'on_monitor_exit', side_effect=exit_fn or noop))
        stack.enter_context(patch('cli.controller.multipassd_governor.Session'))
        await governor.start_async()
    return governor


class TestGovernorStopAsync:
    """Test governor stop_async timeout and cancellation behavior."""

    @pytest.mark.asyncio
    async def test_stop_async_timeout_cancels_monitor_task(self, controller):
        """stop_async should cancel monitor_task if it doesn't complete in 10s."""
        governor = await run_governor(controller(exit_code=None), ready_fn=lambda: True)
        
        async def slow_monitor():
            await asyncio.sleep(3600)
        
        governor.monitor_task = asyncio.create_task(slow_monitor())
        
        with patch('asyncio.wait_for', side_effect=asyncio.TimeoutError):
            await governor.stop_async()
        
        assert governor.monitor_task.cancelled()

    @pytest.mark.asyncio
    async def test_stop_async_waits_for_monitor_task_completion(self, controller):
        """stop_async should wait for monitor_task to complete normally."""
        governor = await run_governor(controller(exit_code=None), ready_fn=lambda: True)
        
        await governor.stop_async()
        
        assert governor.controller.stop_called
        assert governor.monitor_task.done()


class TestGovernorOnMonitorExit:
    """Test governor on_monitor_exit restart logic."""

    @pytest.mark.asyncio
    async def test_on_monitor_exit_cancelled_task_returns_early(self, controller):
        """on_monitor_exit should return early if task was cancelled."""
        governor = MultipassdGovernor(controller(exit_code=42), None, print_daemon_output=False)
        
        task = MagicMock()
        task.cancelled.return_value = True
        
        await governor.on_monitor_exit(task)
        
        assert not governor.controller.start_called

    @pytest.mark.asyncio
    async def test_on_monitor_exit_exit_code_42_no_autorestart_triggers_restart(self, controller):
        """on_monitor_exit should trigger restart on exit code 42 without self-autorestart."""
        ctrl = controller(exit_code=42)
        ctrl.supports_autorestart = False
        
        mock_loop = MagicMock()
        
        governor = MultipassdGovernor(ctrl, mock_loop, print_daemon_output=False)
        
        task = MagicMock()
        task.cancelled.return_value = False
        
        await governor.on_monitor_exit(task)
        
        mock_loop.run_fn.assert_called_once()

    @pytest.mark.asyncio
    async def test_on_monitor_exit_exit_code_42_with_autorestart_no_restart(self, controller):
        """on_monitor_exit should not restart on exit code 42 with self-autorestart."""
        ctrl = controller(exit_code=42)
        ctrl.supports_autorestart = True
        
        governor = MultipassdGovernor(ctrl, MagicMock(), print_daemon_output=False)
        
        task = MagicMock()
        task.cancelled.return_value = False
        
        await governor.on_monitor_exit(task)
        
        assert not ctrl.start_called

    @pytest.mark.asyncio
    async def test_on_monitor_exit_other_exit_code_no_restart(self, controller):
        """on_monitor_exit should not restart on non-42 exit codes."""
        ctrl = controller(exit_code=1)
        ctrl.supports_autorestart = False
        
        governor = MultipassdGovernor(ctrl, MagicMock(), print_daemon_output=False)
        
        task = MagicMock()
        task.cancelled.return_value = False
        
        await governor.on_monitor_exit(task)
        
        assert not ctrl.start_called


class TestGovernorReadStream:
    """Test governor _read_stream error pattern matching."""

    @pytest.mark.asyncio
    async def test_read_stream_matches_dnsmasq_error(self, controller):
        """_read_stream should match dnsmasq binding error."""
        governor = MultipassdGovernor(controller(exit_code=None), None, print_daemon_output=False)
        
        async def mock_output():
            yield "dnsmasq: failed to create listening socket"
        
        governor.controller.follow_output = mock_output
        
        result = await governor._read_stream()
        
        assert "dnsmasq" in result
        assert "port 53" in result

    @pytest.mark.asyncio
    async def test_read_stream_matches_write_lock_error(self, controller):
        """_read_stream should match shared write lock error."""
        governor = MultipassdGovernor(controller(exit_code=None), None, print_daemon_output=False)
        
        async def mock_output():
            yield 'Failed to get shared "write" lock'
        
        governor.controller.follow_output = mock_output
        
        result = await governor._read_stream()
        
        assert "write lock" in result

    @pytest.mark.asyncio
    async def test_read_stream_matches_socket_address_error(self, controller):
        """_read_stream should match socket address in use error."""
        governor = MultipassdGovernor(controller(exit_code=None), None, print_daemon_output=False)
        
        async def mock_output():
            yield "Only one usage of each socket address"
        
        governor.controller.follow_output = mock_output
        
        result = await governor._read_stream()
        
        assert "gRPC port" in result

    @pytest.mark.asyncio
    async def test_read_stream_handles_cancelled_error(self, controller):
        """_read_stream should handle CancelledError gracefully."""
        governor = MultipassdGovernor(controller(exit_code=None), None, print_daemon_output=False)
        
        async def mock_output():
            raise asyncio.CancelledError()
            yield
        
        governor.controller.follow_output = mock_output
        
        result = await governor._read_stream()
        
        assert result is None

    @pytest.mark.asyncio
    async def test_read_stream_returns_none_on_no_match(self, controller):
        """_read_stream should return None when no error patterns match."""
        governor = MultipassdGovernor(controller(exit_code=None), None, print_daemon_output=False)
        
        async def mock_output():
            yield "normal log line"
            yield "another normal line"
        
        governor.controller.follow_output = mock_output
        
        result = await governor._read_stream()
        
        assert result is None


class TestGovernorMonitor:
    """Test governor _monitor task cancellation propagation."""

    @pytest.mark.asyncio
    async def test_monitor_cancellation_propagates_to_stdout_task(self, controller):
        """_monitor cancellation should propagate to stdout_task."""
        governor = MultipassdGovernor(controller(exit_code=None), None, print_daemon_output=False)
        
        async def slow_output():
            await asyncio.sleep(3600)
            yield
        
        governor.controller.follow_output = slow_output
        
        monitor_task = asyncio.create_task(governor._monitor())
        await asyncio.sleep(0.1)
        
        monitor_task.cancel()
        
        try:
            await monitor_task
        except asyncio.CancelledError:
            pass
        
        assert monitor_task.cancelled()
