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

import sys

from .threadutils import BooleanLatch, BackgroundEventLoop
from .functional import retry, wait_for_future
from .privutils import run_as_privileged, get_sudo_tool
from .procutils import send_ctrl_c
from .temputils import TempDirectory
from .textutils import strip_ansi_escape, uuid4_str, is_valid_ipv4_addr
from .treeutils import find_lineage, merge
from .mathutils import is_within_tolerance

if sys.platform == "win32":
    from .pexpectutils import WinptySpawn
