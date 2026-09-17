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

"""Unit tests for AsyncSubprocess context manager."""

import asyncio
import sys
import time
from unittest.mock import AsyncMock, MagicMock, Mock, patch

import pytest

from cli.utilities.threadutils import (
    AsyncSubprocess,
    StdoutAsyncSubprocess,
    SilentAsyncSubprocess,
)


@pytest.fixture
def mock_process():
    proc = MagicMock(spec=asyncio.subprocess.Process)
    proc.returncode = None
    proc.communicate = AsyncMock(return_value=(None, None))
    return proc


class TestAsyncSubprocess:
    """Test AsyncSubprocess context manager with cancellation handling."""

    @pytest.mark.asyncio
    @pytest.mark.parametrize("exit_code", [0, 42])
    async def test_normal_enter_exit(self, exit_code):
        """A process that finishes before context exit should retain its exit code."""
        async with AsyncSubprocess(
            sys.executable,
            "-c",
            f"import sys; sys.stdin.buffer.read(); sys.exit({exit_code})",
            stdin=asyncio.subprocess.PIPE,
        ) as proc:
            assert isinstance(proc, asyncio.subprocess.Process)
            assert proc.returncode is None
            await asyncio.wait_for(proc.communicate(input=b""), timeout=5)

        assert proc.returncode == exit_code

    @pytest.mark.asyncio
    @pytest.mark.parametrize("terminate_times_out", [False, True])
    async def test_cancellation_during_enter(self, mock_process, terminate_times_out):
        """Mid-spawn cancellation should await the process handle and clean it up."""
        spawn_started = asyncio.Event()
        finish_spawn = asyncio.Event()
        context = AsyncSubprocess("mock")
        if terminate_times_out:
            mock_process.communicate.side_effect = [
                asyncio.TimeoutError,
                (None, None),
            ]

        async def spawn(*args, **kwargs):
            spawn_started.set()
            await finish_spawn.wait()
            return mock_process

        with patch(
            "cli.utilities.threadutils.asyncio.create_subprocess_exec",
            side_effect=spawn,
        ):
            task = asyncio.create_task(context.__aenter__())
            await asyncio.wait_for(spawn_started.wait(), timeout=1)
            task.cancel()
            await asyncio.sleep(0)
            finish_spawn.set()

            with pytest.raises(asyncio.CancelledError):
                await asyncio.wait_for(task, timeout=1)

        assert context.proc is mock_process
        mock_process.terminate.assert_called_once_with()
        if terminate_times_out:
            mock_process.kill.assert_called_once_with()
            assert mock_process.communicate.await_count == 2
        else:
            mock_process.kill.assert_not_called()
            mock_process.communicate.assert_awaited_once_with()

    @pytest.mark.asyncio
    async def test_cancellation_during_exit(self, mock_process):
        """Cancellation while exiting should still drain the process."""
        communicating = asyncio.Event()
        context = AsyncSubprocess("mock")
        context.proc = mock_process

        async def communicate():
            if mock_process.communicate.await_count == 1:
                communicating.set()
                await asyncio.Future()
            return None, None

        mock_process.communicate.side_effect = communicate
        task = asyncio.create_task(context.__aexit__(None, None, None))
        await asyncio.wait_for(communicating.wait(), timeout=1)
        task.cancel()

        with pytest.raises(asyncio.CancelledError):
            await asyncio.wait_for(task, timeout=1)

        assert context.proc is None
        mock_process.terminate.assert_called_once_with()
        mock_process.kill.assert_not_called()
        assert mock_process.communicate.await_count == 2

    @pytest.mark.asyncio
    async def test_cancellation_during_body(self, mock_process):
        """Cancellation in the context body should propagate after cleanup."""
        entered = asyncio.Event()
        context = AsyncSubprocess("mock")

        async def run():
            async with context:
                entered.set()
                await asyncio.Future()

        with patch(
            "cli.utilities.threadutils.asyncio.create_subprocess_exec",
            return_value=mock_process,
        ):
            task = asyncio.create_task(run())
            await asyncio.wait_for(entered.wait(), timeout=1)
            task.cancel()

            with pytest.raises(asyncio.CancelledError):
                await asyncio.wait_for(task, timeout=1)

        assert context.proc is None
        mock_process.terminate.assert_called_once_with()
        assert mock_process.communicate.await_count == 2

    @pytest.mark.asyncio
    async def test_long_running_process_terminated_on_exit(self):
        """Long-running process should be terminated on context exit."""
        async with AsyncSubprocess(
            sys.executable, "-c", "import time; time.sleep(3600)"
        ) as proc:
            assert proc.returncode is None
            start = time.monotonic()

        assert time.monotonic() - start < 5
        assert proc.returncode is not None and proc.returncode != 0

    @pytest.mark.asyncio
    async def test_process_killed_after_terminate_timeout(self, monkeypatch):
        """Process should be killed if terminate times out."""
        async with AsyncSubprocess(
            sys.executable, "-c", "import time; time.sleep(3600)"
        ) as proc:
            # Ignore termination on every platform, without a child signal-handler race.
            monkeypatch.setattr(proc, "terminate", Mock())
            monkeypatch.setattr(proc, "kill", Mock(wraps=proc.kill))
            start = time.monotonic()

        assert 5 <= time.monotonic() - start < 10
        proc.terminate.assert_called_once_with()
        proc.kill.assert_called_once_with()
        assert proc.returncode is not None and proc.returncode != 0


class TestStdoutAsyncSubprocess:
    """Test StdoutAsyncSubprocess captures stdout."""

    @pytest.mark.asyncio
    async def test_captures_stdout(self):
        """StdoutAsyncSubprocess should capture stdout."""
        async with StdoutAsyncSubprocess(
            sys.executable, "-c", "print('hello', end='')"
        ) as proc:
            stdout, stderr = await proc.communicate()
            assert stdout == b"hello"
            assert stderr is None
            assert proc.returncode == 0

    @pytest.mark.asyncio
    async def test_stderr_redirected_to_stdout(self):
        """Stderr should be redirected to stdout."""
        async with StdoutAsyncSubprocess(
            sys.executable,
            "-c",
            "import sys; print('out', flush=True); print('err', file=sys.stderr)",
        ) as proc:
            stdout, stderr = await proc.communicate()
            assert stdout.replace(b"\r", b"") == b"out\nerr\n"
            assert stderr is None
            assert proc.returncode == 0


class TestSilentAsyncSubprocess:
    """Test SilentAsyncSubprocess suppresses output."""

    @pytest.mark.asyncio
    async def test_suppresses_all_output(self):
        """SilentAsyncSubprocess should suppress all output."""
        async with SilentAsyncSubprocess(sys.executable, "-c", "print('hello')") as proc:
            stdout, stderr = await proc.communicate()
            assert stdout is None
            assert stderr is None
            assert proc.returncode == 0

    @pytest.mark.asyncio
    async def test_stderr_suppressed(self):
        """Stderr should be suppressed."""
        async with SilentAsyncSubprocess(
            sys.executable, "-c", "import sys; print('err', file=sys.stderr)"
        ) as proc:
            stdout, stderr = await proc.communicate()
            assert stdout is None
            assert stderr is None
            assert proc.returncode == 0
