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

import asyncio
from unittest.mock import patch

from cli.controller.multipassd_controller import MultipassdController
from cli.controller.multipassd_governor import MultipassdGovernor


class MockController(MultipassdController):
    """Simulate either a stop-driven daemon or one that exits on its own.

    With exit_code=None, wait_exit blocks until stop is called and returns zero.
    Otherwise, it returns exit_code after exit_delay, independently of stop.
    """

    def __init__(self, exit_code=None, exit_delay=0.0, supports_autorestart=False):
        self.exit_code_value = exit_code
        self.exit_delay = exit_delay
        self.start_called = False
        self.stop_called = False
        self.supports_autorestart = supports_autorestart
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


async def run_governor(daemon_controller, ready_delay=3600, ready_result=True):
    """Start governor with mocked dependencies."""

    async def ready():
        await asyncio.sleep(ready_delay)
        return ready_result

    governor = MultipassdGovernor(daemon_controller, None, print_daemon_output=False)

    with (
        patch.object(governor, "_ensure_client_certs_are_created"),
        patch.object(governor, "_authenticate_client_cert"),
        patch.object(
            governor,
            "wait_for_multipassd_ready",
            side_effect=ready,
        ),
        patch.object(governor, "on_monitor_exit"),
        patch("cli.controller.multipassd_governor.Session"),
    ):
        await governor.start_async()
    return governor
