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

"""Unit tests for daemon controller auto-restart detection."""

from unittest.mock import AsyncMock, patch

import pytest

from cli.controller.snap_multipassd_controller import SnapMultipassdController
from cli.controller.winsvc_multipassd_controller import (
    WindowsServiceMultipassdController,
)


class TestWaitForSelfAutorestart:
    """Tests for wait_for_self_autorestart() in self-restarting controllers."""

    @pytest.mark.asyncio
    async def test_winsvc_ignores_zero_and_none_pid_until_new_pid(self):
        ctrl = object.__new__(WindowsServiceMultipassdController)
        ctrl._daemon_pid = 1234
        ctrl._get_pid = AsyncMock(side_effect=[1234, 0, None, 0, 5678])

        with patch("asyncio.sleep", new_callable=AsyncMock):
            await ctrl.wait_for_self_autorestart(timeout=5)

        assert ctrl._daemon_pid == 5678
        assert ctrl._get_pid.await_count == 5

    @pytest.mark.asyncio
    async def test_snap_ignores_zero_and_none_timestamp_until_new_timestamp(self):
        ctrl = object.__new__(SnapMultipassdController)
        ctrl._active_enter_timestamp_monotonic = 1000
        ctrl._get_active_enter_timestamp_monotonic = AsyncMock(
            side_effect=[1000, 0, None, 2000]
        )

        with patch("asyncio.sleep", new_callable=AsyncMock):
            await ctrl.wait_for_self_autorestart(timeout=5)

        assert ctrl._active_enter_timestamp_monotonic == 2000
        assert ctrl._get_active_enter_timestamp_monotonic.await_count == 4
