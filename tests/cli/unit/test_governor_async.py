#!/usr/bin/env python3
"""Unit tests for MultipassdGovernor async paths."""

import asyncio
from contextlib import ExitStack
from unittest.mock import AsyncMock, MagicMock, patch

import pytest

from cli.controller.multipassd_governor import MultipassdGovernor
from cli.multipass.exceptions import TestCaseFailure, TestSessionFailure


class MockSubprocess:
    """Mock subprocess for testing wait_for_multipassd_ready."""

    def __init__(self, stdout=b"", returncode=0):
        self.stdout = stdout
        self.returncode = returncode

    async def communicate(self):
        return self.stdout, b""

    async def __aenter__(self):
        return self

    async def __aexit__(self, *args):
        pass


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
        stack.enter_context(
            patch.object(
                governor, "_ensure_client_certs_are_created", new_callable=AsyncMock
            )
        )
        stack.enter_context(patch.object(governor, "_authenticate_client_cert"))
        stack.enter_context(
            patch.object(
                governor,
                "wait_for_multipassd_ready",
                side_effect=ready_fn or hang_forever,
            )
        )
        stack.enter_context(
            patch.object(governor, "on_monitor_exit", side_effect=exit_fn or noop)
        )
        stack.enter_context(patch("cli.controller.multipassd_governor.Session"))
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

        with patch("asyncio.wait_for", side_effect=asyncio.TimeoutError):
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
        governor = MultipassdGovernor(
            controller(exit_code=42), None, print_daemon_output=False
        )

        task = MagicMock()
        task.cancelled.return_value = True

        await governor.on_monitor_exit(task)

        assert not governor.controller.start_called

    @pytest.mark.asyncio
    async def test_on_monitor_exit_exit_code_42_no_autorestart_triggers_restart(
        self, controller
    ):
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
    async def test_on_monitor_exit_exit_code_42_with_autorestart_no_restart(
        self, controller
    ):
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
        governor = MultipassdGovernor(
            controller(exit_code=None), None, print_daemon_output=False
        )

        async def mock_output():
            yield "dnsmasq: failed to create listening socket"

        governor.controller.follow_output = mock_output

        result = await governor._read_stream()

        assert "dnsmasq" in result
        assert "port 53" in result

    @pytest.mark.asyncio
    async def test_read_stream_matches_write_lock_error(self, controller):
        """_read_stream should match shared write lock error."""
        governor = MultipassdGovernor(
            controller(exit_code=None), None, print_daemon_output=False
        )

        async def mock_output():
            yield 'Failed to get shared "write" lock'

        governor.controller.follow_output = mock_output

        result = await governor._read_stream()

        assert "write lock" in result

    @pytest.mark.asyncio
    async def test_read_stream_matches_socket_address_error(self, controller):
        """_read_stream should match socket address in use error."""
        governor = MultipassdGovernor(
            controller(exit_code=None), None, print_daemon_output=False
        )

        async def mock_output():
            yield "Only one usage of each socket address"

        governor.controller.follow_output = mock_output

        result = await governor._read_stream()

        assert "gRPC port" in result

    @pytest.mark.asyncio
    async def test_read_stream_handles_cancelled_error(self, controller):
        """_read_stream should handle CancelledError gracefully."""
        governor = MultipassdGovernor(
            controller(exit_code=None), None, print_daemon_output=False
        )

        async def mock_output():
            raise asyncio.CancelledError()
            yield

        governor.controller.follow_output = mock_output

        result = await governor._read_stream()

        assert result is None

    @pytest.mark.asyncio
    async def test_read_stream_returns_none_on_no_match(self, controller):
        """_read_stream should return None when no error patterns match."""
        governor = MultipassdGovernor(
            controller(exit_code=None), None, print_daemon_output=False
        )

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
        governor = MultipassdGovernor(
            controller(exit_code=None), None, print_daemon_output=False
        )

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

    @pytest.mark.asyncio
    async def test_monitor_includes_error_reason_in_exception(self, controller):
        """_monitor should include error reason from _read_stream in exception message."""
        ctrl = controller(exit_code=2, exit_delay=0.1)
        governor = MultipassdGovernor(ctrl, None, print_daemon_output=False)

        async def error_output():
            yield "dnsmasq: failed to create listening socket"

        governor.controller.follow_output = error_output

        with pytest.raises(TestCaseFailure) as exc_info:
            await governor._monitor()

        assert "multipassd died with code 2" in str(exc_info.value)
        assert "dnsmasq" in str(exc_info.value)
        assert "port 53" in str(exc_info.value)

    @pytest.mark.asyncio
    async def test_monitor_includes_error_reason_in_session_failure(self, controller):
        """_monitor should include error reason in TestSessionFailure for exit code 1."""
        ctrl = controller(exit_code=1, exit_delay=0.1)
        governor = MultipassdGovernor(ctrl, None, print_daemon_output=False)

        async def error_output():
            yield 'Failed to get shared "write" lock'

        governor.controller.follow_output = error_output

        with pytest.raises(TestSessionFailure) as exc_info:
            await governor._monitor()

        assert "multipassd died with code 1" in str(exc_info.value)
        assert "write lock" in str(exc_info.value)

    @pytest.mark.asyncio
    async def test_monitor_no_error_reason_when_stream_returns_none(self, controller):
        """_monitor should not include reason when _read_stream returns None."""
        ctrl = controller(exit_code=2, exit_delay=0.1)
        governor = MultipassdGovernor(ctrl, None, print_daemon_output=False)

        async def normal_output():
            yield "normal log line"

        governor.controller.follow_output = normal_output

        with pytest.raises(TestCaseFailure) as exc_info:
            await governor._monitor()

        assert "multipassd died with code 2" in str(exc_info.value)
        assert "Reason:" not in str(exc_info.value)

    @pytest.mark.asyncio
    async def test_monitor_no_error_reason_when_stream_cancelled(self, controller):
        """_monitor should handle CancelledError from _read_stream gracefully."""
        ctrl = controller(exit_code=2, exit_delay=0.1)
        governor = MultipassdGovernor(ctrl, None, print_daemon_output=False)

        async def cancelling_output():
            await asyncio.sleep(3600)
            yield

        governor.controller.follow_output = cancelling_output

        with pytest.raises(TestCaseFailure) as exc_info:
            await governor._monitor()

        assert "multipassd died with code 2" in str(exc_info.value)
        assert "Reason:" not in str(exc_info.value)


class TestGovernorStartupFailures:
    """Test governor startup failure scenarios."""

    @pytest.mark.asyncio
    async def test_daemon_never_ready_stops_and_exits(self, controller):
        """wait_for_multipassd_ready returning False should stop daemon and exit."""
        ctrl = controller(exit_code=None)

        governor = MultipassdGovernor(ctrl, None, print_daemon_output=False)

        async def never_ready():
            return False

        with ExitStack() as stack:
            stack.enter_context(
                patch.object(
                    governor, "_ensure_client_certs_are_created", new_callable=AsyncMock
                )
            )
            stack.enter_context(patch.object(governor, "_authenticate_client_cert"))
            stack.enter_context(
                patch.object(
                    governor, "wait_for_multipassd_ready", side_effect=never_ready
                )
            )
            stack.enter_context(
                patch(
                    "cli.controller.multipassd_governor.pytest.exit",
                    side_effect=SystemExit(12),
                )
            )

            with pytest.raises(SystemExit) as exc_info:
                await governor.start_async()

            assert exc_info.value.code == 12

        assert ctrl.stop_called

    @pytest.mark.asyncio
    async def test_cert_creation_failure_propagates(self, controller):
        """_ensure_client_certs_are_created failure should propagate."""
        ctrl = controller(exit_code=None)

        governor = MultipassdGovernor(ctrl, None, print_daemon_output=False)

        with ExitStack() as stack:
            stack.enter_context(
                patch.object(
                    governor,
                    "_ensure_client_certs_are_created",
                    new_callable=AsyncMock,
                    side_effect=RuntimeError("cert creation failed"),
                )
            )

            with pytest.raises(RuntimeError, match="cert creation failed"):
                await governor.start_async()

    @pytest.mark.asyncio
    async def test_auth_failure_propagates(self, controller):
        """_authenticate_client_cert failure should propagate."""
        ctrl = controller(exit_code=None)

        governor = MultipassdGovernor(ctrl, None, print_daemon_output=False)

        with ExitStack() as stack:
            stack.enter_context(
                patch.object(
                    governor, "_ensure_client_certs_are_created", new_callable=AsyncMock
                )
            )
            stack.enter_context(
                patch.object(
                    governor,
                    "_authenticate_client_cert",
                    side_effect=RuntimeError("auth failed"),
                )
            )

            with pytest.raises(RuntimeError, match="auth failed"):
                await governor.start_async()


class TestGovernorConcurrentOperations:
    """Test governor concurrent operation scenarios."""

    @pytest.mark.asyncio
    async def test_stop_during_start_causes_startup_failure(self, controller):
        """Calling stop_async during start_async should cause startup failure."""
        ctrl = controller(exit_code=None)

        governor = MultipassdGovernor(ctrl, None, print_daemon_output=False)

        async def slow_ready():
            await asyncio.sleep(1.0)
            return True

        with ExitStack() as stack:
            stack.enter_context(
                patch.object(
                    governor, "_ensure_client_certs_are_created", new_callable=AsyncMock
                )
            )
            stack.enter_context(patch.object(governor, "_authenticate_client_cert"))
            stack.enter_context(
                patch.object(
                    governor, "wait_for_multipassd_ready", side_effect=slow_ready
                )
            )
            stack.enter_context(
                patch.object(governor, "on_monitor_exit", side_effect=lambda t: None)
            )
            stack.enter_context(patch("cli.controller.multipassd_governor.Session"))

            start_task = asyncio.create_task(governor.start_async())
            await asyncio.sleep(0.1)

            await governor.stop_async()

            with pytest.raises(TestCaseFailure, match="daemon exited during startup"):
                await start_task

    @pytest.mark.asyncio
    async def test_restart_cycle_resets_state(self, controller):
        """restart_async should properly reset state between cycles."""
        ctrl = controller(exit_code=None)

        governor = await run_governor(ctrl, ready_fn=lambda: True)

        assert governor.daemon_ready_event.is_set()
        assert governor.monitor_task is not None

        await governor.stop_async()

        assert governor.graceful_exit_initiated
        assert ctrl.stop_called

        governor2 = await run_governor(ctrl, ready_fn=lambda: True)

        assert governor2.daemon_ready_event.is_set()
        assert governor2.monitor_task is not None
        assert not governor2.graceful_exit_initiated


class TestGovernorCrashMidTest:
    """Test governor handling daemon crash during test execution."""

    @pytest.mark.asyncio
    async def test_daemon_crash_with_exit_code_1_aborts_session(self, controller):
        """Daemon exit with code 1 during test should raise TestSessionFailure."""
        ctrl = controller(exit_code=1)

        with pytest.raises(TestSessionFailure) as exc_info:
            await run_governor(ctrl)

        assert "multipassd died with code 1" in str(exc_info.value)

    @pytest.mark.asyncio
    async def test_daemon_crash_with_other_codes_raises_test_case_failure(
        self, controller
    ):
        """Daemon exit with non-1 codes should raise TestCaseFailure."""
        for exit_code in [2, 137, 139]:
            ctrl = controller(exit_code=exit_code)

            with pytest.raises(TestCaseFailure) as exc_info:
                await run_governor(ctrl)

            assert f"multipassd died with code {exit_code}" in str(exc_info.value)


class TestWaitForMultipassdReady:
    """Test wait_for_multipassd_ready static method."""

    @pytest.fixture
    def mock_env(self):
        """Fixture to patch environment-related functions."""
        with (
            patch(
                "cli.controller.multipassd_governor.get_multipass_path",
                return_value="/fake/path",
            ),
            patch(
                "cli.controller.multipassd_governor.get_multipass_env", return_value={}
            ),
        ):
            yield

    @pytest.mark.asyncio
    async def test_successful_when_versions_match(self, mock_env):
        """Returns True when find succeeds and CLI/daemon versions match."""
        find_proc = MockSubprocess(stdout=b"", returncode=0)
        version_proc = MockSubprocess(
            stdout=b"multipass  1.12.0\nmultipassd  1.12.0\n", returncode=0
        )

        with patch(
            "cli.controller.multipassd_governor.StdoutAsyncSubprocess",
            side_effect=[find_proc, version_proc],
        ):
            result = await MultipassdGovernor.wait_for_multipassd_ready(timeout=1)
            assert result is True

    @pytest.mark.asyncio
    async def test_returns_false_on_timeout(self, mock_env):
        """Returns False when daemon doesn't respond within timeout."""

        class TimeoutSubprocess(MockSubprocess):
            async def communicate(self):
                raise asyncio.TimeoutError()

        with patch(
            "cli.controller.multipassd_governor.StdoutAsyncSubprocess",
            return_value=TimeoutSubprocess(),
        ):
            result = await MultipassdGovernor.wait_for_multipassd_ready(timeout=0.5)
            assert result is False

    @pytest.mark.asyncio
    async def test_retries_on_timeout_error(self, mock_env):
        """Retries when subprocess raises TimeoutError."""
        call_count = 0

        class FlakySubprocess(MockSubprocess):
            async def __aenter__(self):
                nonlocal call_count
                call_count += 1
                if call_count < 2:
                    raise asyncio.TimeoutError()
                return MockSubprocess(
                    stdout=b"multipass  1.12.0\nmultipassd  1.12.0\n", returncode=0
                )

        with patch(
            "cli.controller.multipassd_governor.StdoutAsyncSubprocess",
            return_value=FlakySubprocess(),
        ):
            result = await MultipassdGovernor.wait_for_multipassd_ready(timeout=2)
            assert result is True
            assert call_count >= 2

    @pytest.mark.asyncio
    async def test_propagates_cancelled_error(self, mock_env):
        """Propagates CancelledError instead of catching it."""

        class CancelSubprocess(MockSubprocess):
            async def __aenter__(self):
                raise asyncio.CancelledError()

        with patch(
            "cli.controller.multipassd_governor.StdoutAsyncSubprocess",
            return_value=CancelSubprocess(),
        ):
            with pytest.raises(asyncio.CancelledError):
                await MultipassdGovernor.wait_for_multipassd_ready(timeout=1)

    @pytest.mark.asyncio
    async def test_returns_false_when_version_output_insufficient(self, mock_env):
        """Returns False when version output has fewer than 2 lines."""
        find_proc = MockSubprocess(stdout=b"", returncode=0)
        version_proc = MockSubprocess(stdout=b"multipass  1.12.0\n", returncode=0)

        with patch(
            "cli.controller.multipassd_governor.StdoutAsyncSubprocess",
            side_effect=[find_proc, version_proc],
        ):
            result = await MultipassdGovernor.wait_for_multipassd_ready(timeout=1)
            assert result is False
