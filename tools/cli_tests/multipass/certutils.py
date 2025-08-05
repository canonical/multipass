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

import sys
import os
import logging
import shutil
from pathlib import Path


def get_client_cert_path():
    if sys.platform == "win32":
        data_location = Path(
            os.getenv("LOCALAPPDATA", Path.home() / "AppData" / "Local")
        )
    elif sys.platform == "darwin":
        data_location = Path.home() / "Library" / "Application Support"
    else:
        # Not sure about this:
        data_location = Path.home() / ".local" / "share"
    # cat ~/snap/multipass/current/data/multipass-client-certificate/multipass_cert.pem | sudo tee -a /var/snap/multipass/common/data/multipassd/authenticated-certs/multipass_client_certs.pem > /dev/null
    # snap restart multipass
    return data_location / "multipass-client-certificate" / "multipass_cert.pem"


def authenticate_client_cert(client_cert_path, data_root):
    dst_path = (
        Path(data_root) / "data" / "authenticated-certs" / "multipass_client_certs.pem"
    )
    # Ensure the destination directory exists
    dst_path.parent.mkdir(parents=True, exist_ok=True)
    # Ensure the file exists (like `tee -a`, which creates it if missing)
    dst_path.touch(exist_ok=True)
    with Path(client_cert_path).open("rb") as src, dst_path.open("ab") as dst:
        logging.debug(f"auth {client_cert_path} -> {dst_path}")
        shutil.copyfileobj(src, dst)
