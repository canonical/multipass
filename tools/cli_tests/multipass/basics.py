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

import os
import shutil

from cli_tests.config import config


def get_multipass_env():
    multipass_env = os.environ.copy()
    multipass_env["MULTIPASS_STORAGE"] = config.data_root
    return multipass_env


def get_multipass_path():
    return shutil.which("multipass", path=config.build_root)


def get_multipassd_path():
    return shutil.which("multipassd", path=config.build_root)
