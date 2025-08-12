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

    async def start(self) -> None: ...
    async def stop(self, graceful=True) -> None: ...
    async def restart(self) -> None: ...
    async def follow_output(self) -> AsyncIterator[str]:
        """Yield decoded log lines (utf-8, replace errors)."""

    async def is_active(self) -> bool: ...
    async def wait_exit(self) -> Optional[int]:
        """Return exit code if available; else None. Should return promptly if stopped."""

    def supports_self_autorestart(self) -> bool: ...
    async def exit_code(self) -> Optional[int]: ...
