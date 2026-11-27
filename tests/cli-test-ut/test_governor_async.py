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
from unittest.mock import DEFAULT, AsyncMock, MagicMock, patch

import pytest

from cli.controller.multipassd_governor import MultipassdGovernor
from cli.multipass.exceptions import TestCaseFailure, TestSessionFailure
from cli.utilities.threadutils import BackgroundEventLoop
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


class HangingSubprocess(MockSubprocess):
    """A subprocess whose `communicate` never returns on its own."""

    def __init__(self):
        super().__init__()
        self.communicate_calls = 0

    async def communicate(self):
        self.communicate_calls += 1
        await asyncio.Event().wait()


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

        def wait_for(aw, timeout):
            if aw is monitor_task:
                raise TimeoutError
            return DEFAULT

        with patch("asyncio.wait_for", side_effect=wait_for, wraps=asyncio.wait_for):
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

    @pytest.mark.asyncio
    async def test_stop_async_clears_ready_so_health_check_is_skipped(self):
        """A deliberately stopped daemon must not be health-probed (and so
        abort the session) by the next command."""
        governor = await run_governor(MockController(), ready_delay=0)
        assert governor.daemon_ready_event.is_set()

        await governor.stop_async()

        assert not governor.daemon_ready_event.is_set()
        with patch.object(MultipassdGovernor, "_probe_healthy") as probe:
            governor.check_health()
        probe.assert_not_called()


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
        "stream_started", [True, False], ids=["stream-started", "stream-not-started"]
    )
    async def test_monitor_cancelled_while_awaiting_stdout_task(self, stream_started):
        """A cancel landing while _monitor awaits stdout_task must not be swallowed."""
        ctrl = MockController(exit_code=2)
        governor = MultipassdGovernor(ctrl, None, print_daemon_output=False)
        monitor_task = None

        async def follow_output():
            await asyncio.sleep(3600)
            yield ""

        async def wait_exit():
            if stream_started:
                await asyncio.sleep(0.01)
            # Runs after _monitor has cancelled stdout_task and suspended on it.
            asyncio.get_running_loop().call_soon(monitor_task.cancel)
            return 2

        ctrl.follow_output = follow_output
        ctrl.wait_exit = wait_exit
        monitor_task = asyncio.create_task(governor._monitor())

        with pytest.raises(asyncio.CancelledError):
            await monitor_task

        assert monitor_task.cancelled()

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

    @pytest.mark.asyncio
    async def test_fatal_readiness_error_stops_and_exits_with_reason(self):
        """Fatal readiness errors (e.g. version mismatch) surface their reason."""
        ctrl = MockController(exit_code=None)
        governor = MultipassdGovernor(ctrl, None, print_daemon_output=False)
        exit_mock = MagicMock(side_effect=SystemExit(12))

        with (
            patch.object(governor, "_ensure_client_certs_are_created"),
            patch.object(governor, "_authenticate_client_cert"),
            patch.object(
                governor,
                "wait_for_multipassd_ready",
                side_effect=TestSessionFailure(
                    "Version mismatch detected (CLI: 1.0, Daemon: 2.0)."
                ),
            ),
            patch.object(governor, "on_monitor_exit"),
            patch("cli.controller.multipassd_governor.pytest.exit", exit_mock),
        ):
            with pytest.raises(SystemExit) as exc_info:
                await governor.start_async()

            assert exc_info.value.code == 12

        assert ctrl.stop_called
        exit_mock.assert_called_once_with(
            "Tests cannot proceed: Version mismatch detected (CLI: 1.0, Daemon: 2.0).",
            returncode=12,
        )


class TestGovernorUnexpectedExit:
    """Unexpected daemon exits after startup must surface in check_health()."""

    @staticmethod
    async def _start(ctrl):
        governor = MultipassdGovernor(ctrl, None, print_daemon_output=False)
        with (
            patch.object(governor, "_ensure_client_certs_are_created"),
            patch.object(governor, "_authenticate_client_cert"),
            patch.object(
                governor,
                "wait_for_multipassd_ready",
                new_callable=AsyncMock,
                return_value=True,
            ),
        ):
            await governor.start_async()
        return governor

    @staticmethod
    async def _wait_for_monitor_exit(governor, timeout=2):
        async with asyncio.timeout(timeout):
            while governor.monitor_task is not None:
                await asyncio.sleep(0.01)

    @pytest.mark.asyncio
    async def test_check_health_raises_after_crash(self):
        governor = await self._start(MockController(exit_code=2, exit_delay=0.05))
        assert governor.daemon_ready_event.is_set()

        await self._wait_for_monitor_exit(governor)

        assert not governor.daemon_ready_event.is_set()
        for _ in range(2):
            with pytest.raises(TestCaseFailure, match="multipassd died with code 2"):
                governor.check_health()

    @pytest.mark.asyncio
    async def test_graceful_stop_records_no_failure(self):
        governor = await self._start(MockController())

        await governor.stop_async()
        await self._wait_for_monitor_exit(governor)

        assert governor.daemon_failure is None
        governor.check_health()

    @pytest.mark.asyncio
    async def test_successful_start_clears_previous_failure(self):
        ctrl = MockController()
        governor = MultipassdGovernor(ctrl, None, print_daemon_output=False)
        governor.daemon_failure = TestCaseFailure("previous crash")

        with (
            patch.object(governor, "_ensure_client_certs_are_created"),
            patch.object(governor, "_authenticate_client_cert"),
            patch.object(
                governor,
                "wait_for_multipassd_ready",
                new_callable=AsyncMock,
                return_value=True,
            ),
        ):
            await governor.start_async()

        assert governor.daemon_failure is None
        await governor.stop_async()


class TestGovernorHealthCheck:
    """check_health() must tolerate a transiently slow daemon."""

    @pytest.fixture
    def governor(self):
        with BackgroundEventLoop() as loop:
            governor = MultipassdGovernor(MockController(), loop, print_daemon_output=False)
            governor.daemon_ready_event.set()
            yield governor

    @staticmethod
    def install_probes(monkeypatch, outcomes):
        outcomes = iter(outcomes)
        calls = []

        async def probe():
            calls.append(None)
            outcome = next(outcomes)
            if outcome == "hang":
                await asyncio.sleep(3600)
            return outcome

        monkeypatch.setattr(MultipassdGovernor, "_run_version_probe", staticmethod(probe))
        return calls

    def test_recovers_after_transient_failures(self, governor, monkeypatch):
        calls = self.install_probes(
            monkeypatch,
            ["hang", (1, b"connection refused"), (0, b"multipass 1.0\nmultipassd 1.0\n")],
        )

        governor.check_health(timeout=0.05, attempts=3, retry_delay=0)

        assert len(calls) == 3

    def test_aborts_with_reason_after_all_probes_fail(self, governor, monkeypatch):
        calls = self.install_probes(
            monkeypatch, ["hang", "hang", "hang", (0, b"multipass 1.0\n")]
        )
        sleeps = []
        monkeypatch.setattr("cli.utilities.functional.time.sleep", sleeps.append)

        with pytest.raises(
            TestSessionFailure,
            match=r"unresponsive after 4 probes \(last: no multipassd version",
        ):
            governor.check_health(timeout=0.05, attempts=4, retry_delay=0.5)

        assert len(calls) == 4
        assert sleeps == [0.5] * 3


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
    @pytest.mark.parametrize(
        "cli_ver,daemon_ver,expected",
        [
            ("1.17.0-dev.10+g12345678.win", "1.17.0-dev.10+g12345678.win", True),
            ("1.17.0-dev.10+g12345678", "1.17.0-dev.9+g87654321", False),
            ("1.17.0", "1.17.0-dev.10+g12345678", False),
            ("1.17.0+mac", "1.17.0+win", False),
        ],
    )
    async def test_compares_full_version_strings(
        self, mock_env, cli_ver, daemon_ver, expected
    ):
        """Builds of the same release with different suffixes are a mismatch."""
        version_out = f"multipass  {cli_ver}\nmultipassd  {daemon_ver}\n".encode()

        with patch(
            "cli.controller.multipassd_governor.StdoutAsyncSubprocess",
            side_effect=[MockSubprocess(), MockSubprocess(stdout=version_out)],
        ):
            if expected:
                result = await MultipassdGovernor.wait_for_multipassd_ready(timeout=1)
                assert result is True
            else:
                with pytest.raises(TestSessionFailure, match="Version mismatch detected"):
                    await MultipassdGovernor.wait_for_multipassd_ready(timeout=1)

    @pytest.mark.asyncio
    async def test_unauthenticated_client_raises(self, mock_env):
        """Unauthenticated client raises TestSessionFailure immediately."""
        unauth_proc = MockSubprocess(
            stdout=b"The client is not authenticated with the Multipass service",
            returncode=1,
        )

        with patch(
            "cli.controller.multipassd_governor.StdoutAsyncSubprocess",
            return_value=unauth_proc,
        ):
            with pytest.raises(TestSessionFailure, match="Client is not authenticated"):
                await MultipassdGovernor.wait_for_multipassd_ready(timeout=1)

    @pytest.mark.asyncio
    async def test_returns_false_on_timeout(self, mock_env):
        """Returns False when daemon doesn't respond within timeout."""
        hanging_proc = HangingSubprocess()

        with patch(
            "cli.controller.multipassd_governor.StdoutAsyncSubprocess",
            return_value=hanging_proc,
        ):
            result = await MultipassdGovernor.wait_for_multipassd_ready(timeout=0.2)

        assert result is False
        # The deadline cuts the single hanging attempt short; no retry follows.
        assert hanging_proc.communicate_calls == 1

    @pytest.mark.asyncio
    async def test_retries_on_timeout_error(self, mock_env):
        """Retries when subprocess raises TimeoutError."""
        commands = []

        def subprocess(*args, **kwargs):
            command = args[1]
            commands.append(command)
            if command == "images" and commands.count("images") == 1:
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
        assert commands == ["images", "images", "version"]

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
            # Hang on the third attempt so the deadline ends the loop.
            if commands.count("images") == 3:
                return HangingSubprocess()
            return MockSubprocess()

        with (
            patch(
                "cli.controller.multipassd_governor.StdoutAsyncSubprocess",
                side_effect=subprocess,
            ),
            patch(
                "cli.controller.multipassd_governor.asyncio.sleep",
                new_callable=AsyncMock,
            ),
        ):
            result = await MultipassdGovernor.wait_for_multipassd_ready(timeout=0.5)

        assert result is False
        assert commands == ["images", "version", "images", "version", "images"]

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
