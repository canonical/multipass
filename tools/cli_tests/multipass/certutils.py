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

"""Helpers for locating and appending Multipass certificates."""

import logging
import os
import shutil
import sys
from pathlib import Path

from cli_tests.config import config

def get_client_cert_path():
    """Return the default location of the Multipass client certificate for the current OS."""
    if sys.platform == "win32":
        data_location = Path(
            os.getenv("LOCALAPPDATA", Path.home() / "AppData" / "Local")
        )
    elif sys.platform == "darwin":
        data_location = Path.home() / "Library" / "Application Support"
    else:
        if config.daemon_controller == "snapd":
            data_location = Path.home() / "snap" / "multipass" / "current" / "data"
        else:
            # Not sure about this:
            data_location = Path.home() / ".local" / "share"

    return data_location / "multipass-client-certificate" / "multipass_cert.pem"


def authenticate_client_cert(client_cert_path, data_root):
    """
    Append the given client certificate to the authenticated certs bundle
    under <data_root>/data/authenticated-certs/multipass_client_certs.pem.
    """

    dst_path = (
        Path(data_root) / "authenticated-certs" / "multipass_client_certs.pem"
    )
   
    # Ensure the destination directory exists
    dst_path.parent.mkdir(parents=True, exist_ok=True)
    # Ensure the file exists (like `tee -a`, which creates it if missing)
    dst_path.touch(exist_ok=True)
    with Path(client_cert_path).open("rb") as src, dst_path.open("ab") as dst:
        logging.debug(f"auth {client_cert_path} -> {dst_path}")
        shutil.copyfileobj(src, dst)
