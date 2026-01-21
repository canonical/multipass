#!/usr/bin/env python3
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

from typing import AsyncIterator, Protocol, Optional


class MultipassdController(Protocol):
    """Abstracts how multipassd is run/controlled."""

    @staticmethod
    def setup_environment() -> None:
        """Static environment preparation (per-session)."""

    @staticmethod
    def teardown_environment() -> None:
        """Static environment teardown (per-session)."""

    async def start(self) -> None:
        """Start the daemon."""

    async def stop(self, graceful=True) -> None:
        """Stop the daemon."""

    async def restart(self) -> None:
        """Restart the daemon."""

    async def follow_output(self) -> AsyncIterator[str]:
        """Yield decoded log lines (utf-8, replace errors)."""

    async def is_active(self) -> bool:
        """Check if the daemon is active."""

    async def wait_exit(self) -> Optional[int]:
        """Return exit code if available; else None. Should return promptly if stopped."""

    def supports_self_autorestart(self) -> bool:
        """True if the entity that is controlling the daemon is capable of
        autorestarting the daemon when needed (e.g. configuration change)"""

    async def wait_for_self_autorestart(self) -> None:
        """Block until the daemon is back online after an autorestart."""

    async def exit_code(self) -> Optional[int]:
        """Retrieve the last exit code."""
