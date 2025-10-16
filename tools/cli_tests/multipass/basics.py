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

# temporary

def default_storage_dir_for_backend(backend):
    if backend == "snap":
        return "/var/snap/multipass/common/data/multipassd"
    if backend == "launchd":
        return "/var/root/Library/Application Support/multipassd"
    if backend == "winsvc":
        progdata = os.getenv("PROGRAMDATA")
        if not progdata:
            raise RuntimeError("Cannot get %PROGRAMDATA% path!")
        return str(Path(progdata) / "Multipass")
    if backend == "standalone":
        assert False, "The storage directory must be explicitly specified when `standalone` backend is used!"
    raise RuntimeError(f"No default storage directory defined for daemon backend {backend}!")

def determine_storage_dir():
    # Use the specified storage dir when it's explicitly specified
    if config.storage_dir:
        return config.storage_dir

    return default_storage_dir_for_backend(config.daemon_controller)

def determine_data_dir():
    if config.daemon_controller == "standalone":
        return str(Path(determine_storage_dir()) / "data")
    if config.daemon_controller == "snap":
        return determine_storage_dir()
    if config.daemon_controller == "launchd":
        return determine_storage_dir()
    if config.daemon_controller == "winsvc":
        return str(Path(determine_storage_dir()) / "data")
    raise RuntimeError(f"No data root directory defined for daemon backend {config.daemon_controller}!")


def determine_bin_dir():
    if config.bin_dir:
        return config.bin_dir

    if config.daemon_controller == "standalone":
        raise RuntimeError("--bin-dir must be explicitly provided when 'standalone' backend is used!")

    if config.daemon_controller == "snap":
        return "/snap/bin"
    if config.daemon_controller == "launchd":
        return "/Library/Application Support/com.canonical.multipass/bin"
    if config.daemon_controller == "winsvc":
        # Use environment
        return None

def get_multipass_env():
    """Return an environment dict for running Multipass with a custom storage root."""
    multipass_env = os.environ.copy()
    if config.daemon_controller == "standalone" or config.storage_dir != default_storage_dir_for_backend(config.daemon_controller):
        multipass_env["MULTIPASS_STORAGE"] = config.storage_dir
    return multipass_env


def get_multipass_path():
    """Resolve the 'multipass' binary."""
    return shutil.which("multipass", path=config.bin_dir)


def get_multipassd_path():
    """Resolve the 'multipassd' binary."""
    return shutil.which("multipassd", path=config.bin_dir)
