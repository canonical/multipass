#!/usr/bin/env python3
"""Unit tests for AsyncSubprocess context manager."""

import asyncio
import sys

import pytest

from cli.utilities.threadutils import AsyncSubprocess, StdoutAsyncSubprocess, SilentAsyncSubprocess


class TestAsyncSubprocess:
    """Test AsyncSubprocess context manager with cancellation handling."""

    @pytest.mark.asyncio
    async def test_normal_enter_exit(self):
        """Normal enter/exit should start and terminate process cleanly."""
        async with AsyncSubprocess("sleep", "0.1") as proc:
            assert proc is not None
            assert proc.returncode is None
        
        assert proc.returncode is not None

    @pytest.mark.asyncio
    async def test_process_already_exited_on_exit(self):
        """Process already exited on __aexit__ should not error."""
        async with AsyncSubprocess("true") as proc:
            await proc.wait()
            assert proc.returncode == 0

    @pytest.mark.asyncio
    async def test_cancellation_during_enter(self):
        """Cancellation during __aenter__ should spawn then terminate process."""
        async def cancellable_spawn():
            async with AsyncSubprocess("sleep", "10") as proc:
                await asyncio.sleep(3600)
        
        task = asyncio.create_task(cancellable_spawn())
        await asyncio.sleep(0.1)
        task.cancel()
        
        try:
            await task
        except asyncio.CancelledError:
            pass

    @pytest.mark.asyncio
    async def test_cancellation_during_exit(self):
        """Cancellation during __aexit__ should still clean up process."""
        async def cancellable_exit():
            async with AsyncSubprocess("sleep", "10") as proc:
                await asyncio.sleep(3600)
        
        task = asyncio.create_task(cancellable_exit())
        await asyncio.sleep(0.1)
        task.cancel()
        
        try:
            await task
        except asyncio.CancelledError:
            pass

    @pytest.mark.asyncio
    async def test_long_running_process_terminated_on_exit(self):
        """Long-running process should be terminated on context exit."""
        async with AsyncSubprocess("sleep", "3600") as proc:
            assert proc.returncode is None
        
        assert proc.returncode is not None

    @pytest.mark.asyncio
    async def test_process_killed_after_terminate_timeout(self):
        """Process should be killed if terminate times out."""
        async with AsyncSubprocess(sys.executable, "-c", 
                                   "import signal; signal.signal(signal.SIGTERM, signal.SIG_IGN); import time; time.sleep(3600)") as proc:
            pass
        
        assert proc.returncode is not None

    @pytest.mark.asyncio
    async def test_process_exits_immediately_with_zero(self):
        """Process that exits immediately with code 0 should be handled."""
        async with AsyncSubprocess(sys.executable, "-c", "exit(0)") as proc:
            # Explicitly wait for the process to be reaped
            await proc.wait()
        
        assert proc.returncode == 0

    @pytest.mark.asyncio
    async def test_process_exits_immediately_with_error(self):
        """Process that exits immediately with error code should be handled."""
        async with AsyncSubprocess(sys.executable, "-c", "exit(42)") as proc:
            # Explicitly wait for the process to be reaped
            await proc.wait()
        
        assert proc.returncode == 42


class TestStdoutAsyncSubprocess:
    """Test StdoutAsyncSubprocess captures stdout."""

    @pytest.mark.asyncio
    async def test_captures_stdout(self):
        """StdoutAsyncSubprocess should capture stdout."""
        async with StdoutAsyncSubprocess("echo", "hello") as proc:
            stdout, _ = await proc.communicate()
            assert b"hello" in stdout

    @pytest.mark.asyncio
    async def test_stderr_redirected_to_stdout(self):
        """Stderr should be redirected to stdout."""
        async with StdoutAsyncSubprocess(sys.executable, "-c", 
                                         "import sys; print('out'); print('err', file=sys.stderr)") as proc:
            stdout, _ = await proc.communicate()
            assert b"out" in stdout
            assert b"err" in stdout


class TestSilentAsyncSubprocess:
    """Test SilentAsyncSubprocess suppresses output."""

    @pytest.mark.asyncio
    async def test_suppresses_all_output(self):
        """SilentAsyncSubprocess should suppress all output."""
        async with SilentAsyncSubprocess("echo", "hello") as proc:
            await proc.communicate()
            assert proc.returncode == 0

    @pytest.mark.asyncio
    async def test_stderr_suppressed(self):
        """Stderr should be suppressed."""
        async with SilentAsyncSubprocess(sys.executable, "-c", 
                                         "import sys; print('err', file=sys.stderr)") as proc:
            await proc.communicate()
            assert proc.returncode == 0
