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

"""Helpers for resolving Multipass executables and environment settings."""

import os
import sys
import shutil
from pathlib import Path

from cli_tests.config import config


SNAP_MULTIPASSD_STORAGE = "/var/snap/multipass/common/data/multipassd"
LAUNCHD_MULTIPASSD_STORAGE = "/var/root/Library/Application Support/multipassd"
WIN_MULTIPASSD_STORAGE = str(Path(os.getenv("PROGRAMDATA")) / "Multipass") if sys.platform == "win32" else None
SNAP_BIN_DIR = "/snap/bin"
LAUNCHD_MULTIPASS_BIN_DIR = "/Library/Application Support/com.canonical.multipass/bin"
WIN_BIN_DIR = None # Use env

def get_multipass_env():
    """Return an environment dict for running Multipass with a custom storage root."""
    multipass_env = os.environ.copy()
    if config.data_root and config.data_root != SNAP_MULTIPASSD_STORAGE:
        multipass_env["MULTIPASS_STORAGE"] = config.data_root
    return multipass_env


def get_multipass_path():
    """Resolve the 'multipass' binary."""
    return shutil.which("multipass", path=config.bin_dir)


def get_multipassd_path():
    """Resolve the 'multipassd' binary."""
    return shutil.which("multipassd", path=config.bin_dir)
