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

"""Unit tests for MultipassdGovernor to verify the fix for cascading CancelledError."""

import asyncio
from unittest.mock import patch

import pytest

from cli.controller.multipassd_governor import MultipassdGovernor
from cli.multipass.exceptions import TestCaseFailure, TestSessionFailure


class MockController:
    """Mock controller that simulates daemon behavior."""

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


@pytest.mark.asyncio
@pytest.mark.parametrize(
    "exit_code,exception,message",
    [
        (42, TestCaseFailure, "daemon exited during startup"),
        (0, TestCaseFailure, "multipassd died with code 0"),
        (1, TestSessionFailure, "multipassd died with code 1"),
        (2, TestCaseFailure, "multipassd died with code 2"),
    ],
)
async def test_startup_failures(controller, exit_code, exception, message):
    """Test governor behavior when daemon exits during startup."""
    with pytest.raises(exception) as exc_info:
        await run_governor(controller(exit_code=exit_code))
    assert message in str(exc_info.value)


@pytest.mark.asyncio
async def test_successful_startup(controller):
    """Successful daemon startup should complete without errors."""
    governor = await run_governor(controller(exit_code=None), ready_fn=lambda: True)

    assert governor.monitor_task is not None
    assert governor.daemon_ready_event.is_set()
    assert not governor.daemon_stopped_event.is_set()
    assert governor.controller.start_called

    await governor.stop_async()


@pytest.mark.asyncio
async def test_stop_after_successful_startup(controller):
    """Stopping governor after successful startup should work cleanly."""
    governor = await run_governor(controller(exit_code=None), ready_fn=lambda: True)
    await governor.stop_async()
    assert governor.controller.stop_called
    assert not governor.daemon_ready_event.is_set()
    assert governor.daemon_stopped_event.is_set()


@pytest.mark.asyncio
async def test_no_cancelled_error_on_exit_code_42(controller):
    """Ensure CancelledError is not raised when daemon exits with code 42."""
    with pytest.raises(TestCaseFailure):
        await run_governor(controller(exit_code=42))
