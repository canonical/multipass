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
from governor_helpers import MockController, run_governor


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


class TestGovernorStopAsync:
    """Test governor stop_async timeout and cancellation behavior."""

    @pytest.mark.asyncio
    async def test_stop_async_timeout_cancels_monitor_task(self):
        """stop_async should cancel monitor_task if it doesn't complete in 10s."""
        governor = MultipassdGovernor(
            MockController(), None, print_daemon_output=False
        )

        async def slow_monitor():
            await asyncio.sleep(3600)

        monitor_task = asyncio.create_task(slow_monitor())
        governor.monitor_task = monitor_task

        with patch("asyncio.wait_for", side_effect=asyncio.TimeoutError):
            await governor.stop_async()

            assert governor.controller.stop_called
            assert monitor_task.cancelled()

    @pytest.mark.asyncio
    async def test_stop_async_waits_for_monitor_task_completion(self):
        """stop_async should wait for monitor_task to complete normally."""
        governor = await run_governor(MockController(), ready_delay=0)
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
        cancelled,
        exit_code,
        supports_autorestart,
        expects_restart,
    ):
        """on_monitor_exit should restart only for an unhandled settings change."""
        ctrl = MockController(
            exit_code=exit_code, supports_autorestart=supports_autorestart
        )
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
    @pytest.mark.parametrize(
        "lines,expected",
        [
            (
                ["dnsmasq: failed to create listening socket"],
                "Could not bind dnsmasq to port 53, is there another process running?",
            ),
            (
                ['Failed to get shared "write" lock'],
                "Cannot open an image file for writing, is another process holding "
                "a write lock?",
            ),
            (
                ["Only one usage of each socket address"],
                "Could not bind gRPC port -- is there another daemon process running?",
            ),
            (["normal log line", "another normal line"], None),
        ],
    )
    async def test_read_stream(self, lines, expected):
        """_read_stream should return the reason matching daemon output."""
        governor = MultipassdGovernor(
            MockController(exit_code=None), None, print_daemon_output=False
        )

        async def mock_output():
            for line in lines:
                yield line

        governor.controller.follow_output = mock_output

        assert await governor._read_stream() == expected

    @pytest.mark.asyncio
    async def test_read_stream_handles_cancelled_error(self):
        """_read_stream should handle CancelledError gracefully."""
        governor = MultipassdGovernor(
            MockController(exit_code=None), None, print_daemon_output=False
        )

        async def mock_output():
            raise asyncio.CancelledError()
            yield

        governor.controller.follow_output = mock_output

        result = await governor._read_stream()

        assert result is None

class TestGovernorMonitor:
    """Test governor _monitor task cancellation propagation."""

    @pytest.mark.asyncio
    async def test_monitor_cancellation_propagates_to_stdout_task(self):
        """_monitor cancellation should propagate to stdout_task."""
        governor = MultipassdGovernor(
            MockController(exit_code=None), None, print_daemon_output=False
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
        self, exit_code, error_reason, exception
    ):
        """_monitor should report the daemon exit code and any stream error reason."""
        governor = MultipassdGovernor(
            MockController(exit_code=exit_code, exit_delay=0.01),
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
    async def test_daemon_never_ready_stops_and_exits(self):
        """Failed readiness should stop and drain the daemon before exiting."""
        ctrl = MockController(exit_code=None)
        monitor_task = None
        original_monitor = MultipassdGovernor._monitor

        async def monitor(governor):
            nonlocal monitor_task
            monitor_task = asyncio.current_task()
            return await original_monitor(governor)

        with (
            patch.object(MultipassdGovernor, "_monitor", new=monitor),
            patch(
                "cli.controller.multipassd_governor.pytest.exit",
                side_effect=SystemExit(12),
            ),
        ):
            with pytest.raises(SystemExit) as exc_info:
                await run_governor(ctrl, ready_delay=0, ready_result=False)

            assert exc_info.value.code == 12

        assert ctrl.stop_called
        assert monitor_task is not None
        assert monitor_task.done()
        assert monitor_task.result() is None

class TestGovernorConcurrentOperations:
    """Test governor concurrent operation scenarios."""

    @pytest.mark.asyncio
    async def test_stop_during_start_causes_startup_failure(self):
        """Calling stop_async during start_async should cause startup failure."""
        ctrl = MockController(exit_code=None)
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
