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


async def run_governor(daemon_controller, ready_fn=None, exit_fn=None):
    """Start governor with mocked dependencies."""

    async def hang_forever():
        await asyncio.sleep(3600)

    async def noop(*args, **kwargs):
        pass

    governor = MultipassdGovernor(
        daemon_controller, None, print_daemon_output=False
    )
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
