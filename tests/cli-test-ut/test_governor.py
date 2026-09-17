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

import re

import pytest

from cli.multipass.exceptions import TestCaseFailure, TestSessionFailure
from governor_helpers import MockController, run_governor


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
async def test_startup_failures(exit_code, exception, message):
    """Test governor behavior when daemon exits during startup."""
    with pytest.raises(exception, match=re.escape(message)):
        await run_governor(MockController(exit_code=exit_code))


@pytest.mark.asyncio
async def test_successful_startup_and_shutdown():
    """Daemon startup and shutdown should complete cleanly."""
    governor = await run_governor(MockController(exit_code=None), ready_fn=lambda: True)

    assert governor.monitor_task is not None
    assert governor.daemon_ready_event.is_set()
    assert not governor.daemon_stopped_event.is_set()
    assert governor.controller.start_called

    await governor.stop_async()

    assert governor.controller.stop_called
    assert not governor.daemon_ready_event.is_set()
    assert governor.daemon_stopped_event.is_set()


@pytest.mark.asyncio
async def test_no_cancelled_error_on_exit_code_42():
    """Ensure CancelledError is not raised when daemon exits with code 42."""
    with pytest.raises(TestCaseFailure):
        await run_governor(MockController(exit_code=42))
