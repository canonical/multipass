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

"""Unit tests for MultipassdGovernor async paths."""

import asyncio
import re
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
    ready_side_effect = ready_fn or hang_forever
    if not asyncio.iscoroutinefunction(ready_side_effect):
        sync_ready = ready_side_effect

        async def ready_side_effect(*args, **kwargs):
            result = sync_ready(*args, **kwargs)
            if asyncio.iscoroutine(result):
                return await result
            return result

    with (
        patch.object(governor, "_ensure_client_certs_are_created"),
        patch.object(governor, "_authenticate_client_cert"),
        patch.object(
            governor,
            "wait_for_multipassd_ready",
            side_effect=ready_side_effect,
        ),
        patch.object(governor, "on_monitor_exit", side_effect=exit_fn or noop),
        patch("cli.controller.multipassd_governor.Session"),
    ):
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
        monitor_task = governor.monitor_task

        await governor.stop_async()

        assert governor.controller.stop_called
        assert monitor_task.done()


class TestGovernorOnMonitorExit:
    """Test governor on_monitor_exit restart logic."""

    @pytest.mark.asyncio
    @pytest.mark.parametrize(
        "cancelled,exit_code,supports_autorestart,expects_restart",
        [
            (True, 42, False, False),
            (False, 42, False, True),
            (False, 42, True, False),
            (False, 1, False, False),
        ],
    )
    async def test_on_monitor_exit_restart_logic(
        self,
        controller,
        cancelled,
        exit_code,
        supports_autorestart,
        expects_restart,
    ):
        """on_monitor_exit should restart only for an unhandled settings change."""
        ctrl = controller(exit_code=exit_code)
        ctrl.supports_autorestart = supports_autorestart
        mock_loop = MagicMock()
        governor = MultipassdGovernor(ctrl, mock_loop, print_daemon_output=False)
        task = MagicMock(**{"cancelled.return_value": cancelled})

        await governor.on_monitor_exit(task)

        assert not ctrl.start_called
        if expects_restart:
            mock_loop.run_fn.assert_called_once()
        else:
            mock_loop.run_fn.assert_not_called()


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
        read_stream_cancelled = asyncio.Event()

        async def slow_read_stream():
            try:
                await asyncio.sleep(3600)
            except asyncio.CancelledError:
                read_stream_cancelled.set()
                raise

        with patch.object(governor, "_read_stream", side_effect=slow_read_stream):
            monitor_task = asyncio.create_task(governor._monitor())
            await asyncio.sleep(0)

            monitor_task.cancel()

            with pytest.raises(asyncio.CancelledError):
                await monitor_task

        assert monitor_task.cancelled()
        assert read_stream_cancelled.is_set()

    @pytest.mark.asyncio
    @pytest.mark.parametrize(
        "exit_code,error_reason,exception",
        [
            (
                2,
                "Could not bind dnsmasq to port 53, is there another process running?",
                TestCaseFailure,
            ),
            (
                1,
                "Cannot open an image file for writing, is another process holding a write lock?",
                TestSessionFailure,
            ),
            (2, None, TestCaseFailure),
        ],
    )
    async def test_monitor_reports_daemon_failure(
        self, controller, exit_code, error_reason, exception
    ):
        """_monitor should report the daemon exit code and any stream error reason."""
        governor = MultipassdGovernor(
            controller(exit_code=exit_code, exit_delay=0.01),
            None,
            print_daemon_output=False,
        )
        expected = f"FATAL: multipassd died with code {exit_code}!"
        if error_reason:
            expected += f"\nReason: {error_reason}"

        with (
            patch.object(governor, "_read_stream", return_value=error_reason),
            pytest.raises(exception, match=re.escape(expected)),
        ):
            await governor._monitor()


class TestGovernorStartupFailures:
    """Test governor startup failure scenarios."""

    @pytest.mark.asyncio
    async def test_daemon_never_ready_stops_and_exits(self, controller):
        """wait_for_multipassd_ready returning False should stop daemon and exit."""
        ctrl = controller(exit_code=None)

        with patch(
            "cli.controller.multipassd_governor.pytest.exit",
            side_effect=SystemExit(12),
        ):
            with pytest.raises(SystemExit) as exc_info:
                await run_governor(ctrl, ready_fn=lambda: False)

            assert exc_info.value.code == 12

        assert ctrl.stop_called

    @pytest.mark.asyncio
    async def test_cert_creation_failure_propagates(self, controller):
        """_ensure_client_certs_are_created failure should propagate."""
        ctrl = controller(exit_code=None)

        governor = MultipassdGovernor(ctrl, None, print_daemon_output=False)

        with patch.object(
            governor,
            "_ensure_client_certs_are_created",
            side_effect=RuntimeError("cert creation failed"),
        ):
            with pytest.raises(RuntimeError, match="cert creation failed"):
                await governor.start_async()

    @pytest.mark.asyncio
    async def test_auth_failure_propagates(self, controller):
        """_authenticate_client_cert failure should propagate."""
        ctrl = controller(exit_code=None)

        governor = MultipassdGovernor(ctrl, None, print_daemon_output=False)

        with (
            patch.object(governor, "_ensure_client_certs_are_created"),
            patch.object(
                governor,
                "_authenticate_client_cert",
                side_effect=RuntimeError("auth failed"),
            ),
        ):
            with pytest.raises(RuntimeError, match="auth failed"):
                await governor.start_async()


class TestGovernorConcurrentOperations:
    """Test governor concurrent operation scenarios."""

    @pytest.mark.asyncio
    async def test_stop_during_start_causes_startup_failure(self, controller):
        """Calling stop_async during start_async should cause startup failure."""
        ctrl = controller(exit_code=None)
        governor = MultipassdGovernor(ctrl, None, print_daemon_output=False)
        ready_cancelled = asyncio.Event()

        async def slow_ready():
            try:
                await asyncio.sleep(3600)
            except asyncio.CancelledError:
                ready_cancelled.set()
                raise

        with (
            patch.object(governor, "_ensure_client_certs_are_created"),
            patch.object(governor, "_authenticate_client_cert"),
            patch.object(
                governor, "wait_for_multipassd_ready", side_effect=slow_ready
            ),
            patch.object(governor, "on_monitor_exit"),
            patch("cli.controller.multipassd_governor.Session"),
        ):
            start_task = asyncio.create_task(governor.start_async())
            await asyncio.sleep(0)

            await governor.stop_async()

            with pytest.raises(TestCaseFailure, match="daemon exited during startup"):
                await start_task

        assert ready_cancelled.is_set()
        assert not governor.daemon_ready_event.is_set()

    @pytest.mark.asyncio
    async def test_stop_resets_state(self, controller):
        """stop_async should leave the governor in its stopped state."""
        ctrl = controller(exit_code=None)

        governor = await run_governor(ctrl, ready_fn=lambda: True)

        assert governor.daemon_ready_event.is_set()
        assert governor.monitor_task is not None

        await governor.stop_async()

        assert ctrl.stop_called
        assert not governor.daemon_ready_event.is_set()
        assert governor.monitor_task is None
        assert not governor.graceful_exit_initiated


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
        timeout_proc = MockSubprocess()
        timeout_proc.communicate = AsyncMock(side_effect=asyncio.TimeoutError)

        with (
            patch(
                "cli.controller.multipassd_governor.StdoutAsyncSubprocess",
                return_value=timeout_proc,
            ),
            patch(
                "cli.controller.multipassd_governor.time.time",
                side_effect=[0, 0, 1],
            ),
        ):
            result = await MultipassdGovernor.wait_for_multipassd_ready(timeout=0.5)

        assert result is False
        timeout_proc.communicate.assert_awaited_once()

    @pytest.mark.asyncio
    async def test_retries_on_timeout_error(self, mock_env):
        """Retries when subprocess raises TimeoutError."""
        commands = []

        def subprocess(*args, **kwargs):
            command = args[1]
            commands.append(command)
            if command == "find" and commands.count("find") == 1:
                proc = MockSubprocess()
                proc.communicate = AsyncMock(side_effect=asyncio.TimeoutError)
                return proc
            if command == "version":
                return MockSubprocess(
                    stdout=b"multipass  1.12.0\nmultipassd  1.12.0\n"
                )
            return MockSubprocess()

        with patch(
            "cli.controller.multipassd_governor.StdoutAsyncSubprocess",
            side_effect=subprocess,
        ):
            result = await MultipassdGovernor.wait_for_multipassd_ready(timeout=2)

        assert result is True
        assert commands == ["find", "find", "version"]

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
        """Returns False when daemon version output is absent."""
        commands = []

        def subprocess(*args, **kwargs):
            command = args[1]
            commands.append(command)
            if command == "version":
                return MockSubprocess(stdout=b"multipass  1.12.0\n")
            return MockSubprocess()

        with (
            patch(
                "cli.controller.multipassd_governor.StdoutAsyncSubprocess",
                side_effect=subprocess,
            ),
            patch(
                "cli.controller.multipassd_governor.time.time",
                side_effect=[0, 0, 0, 1],
            ),
            patch(
                "cli.controller.multipassd_governor.asyncio.sleep",
                new_callable=AsyncMock,
            ),
        ):
            result = await MultipassdGovernor.wait_for_multipassd_ready(timeout=1)

        assert result is False
        assert commands == ["find", "version", "find", "version"]

    @pytest.mark.asyncio
    async def test_missing_cli_version_raises(self, mock_env):
        """Missing CLI version output should be a hard failure."""

        def subprocess(*args, **kwargs):
            if args[1] == "version":
                return MockSubprocess(stdout=b"multipassd  1.12.0\n")
            return MockSubprocess()

        with (
            patch(
                "cli.controller.multipassd_governor.StdoutAsyncSubprocess",
                side_effect=subprocess,
            ),
            pytest.raises(AssertionError, match="Could not extract Multipass version"),
        ):
            await MultipassdGovernor.wait_for_multipassd_ready(timeout=1)

    @pytest.mark.asyncio
    async def test_malformed_daemon_version_raises(self, mock_env):
        """A present but malformed daemon version should be a hard failure."""

        def subprocess(*args, **kwargs):
            if args[1] == "version":
                return MockSubprocess(
                    stdout=b"multipass  1.12.0\nmultipassd  unknown\n"
                )
            return MockSubprocess()

        with (
            patch(
                "cli.controller.multipassd_governor.StdoutAsyncSubprocess",
                side_effect=subprocess,
            ),
            pytest.raises(
                AssertionError, match="Could not extract multipassd version"
            ),
        ):
            await MultipassdGovernor.wait_for_multipassd_ready(timeout=1)
