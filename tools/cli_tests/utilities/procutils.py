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

import subprocess
import sys


def send_ctrl_c(pid):
    if sys.platform == "win32":
        subprocess.run(
            f'python -c "import ctypes; k=ctypes.windll.kernel32; k.FreeConsole(); k.AttachConsole({pid}); k.GenerateConsoleCtrlEvent(0, 0)"',
            shell=False,
        )
    else:
        raise NotImplementedError
