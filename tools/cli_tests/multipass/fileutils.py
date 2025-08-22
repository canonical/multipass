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

import shutil
import contextlib
import sys
import subprocess
from pathlib import Path

from .basics import SNAP_MULTIPASSD_STORAGE, LAUNCHD_MULTIPASSD_STORAGE, WIN_MULTIPASSD_STORAGE


def _nuke_vbox_frontends():
    if sys.platform != "win32":
        return
    # We cannot remove the instances folder while the instances are in use.
    subprocess.run(["taskkill", "/IM", "VBoxHeadless.exe", "/F"], check=False)

def nuke_all_instances(data_root, driver):
    """Remove the instances directory and clean all instance records at the
    specified data root."""

    data_root = Path(data_root)

    backend_dirs = []

    if driver == "virtualbox":
        _nuke_vbox_frontends()

    if data_root not in [
        Path(SNAP_MULTIPASSD_STORAGE),
        Path(LAUNCHD_MULTIPASSD_STORAGE),
    ]:
        data_root /= "data"

    if data_root in [Path(LAUNCHD_MULTIPASSD_STORAGE), Path(WIN_MULTIPASSD_STORAGE) / "data"]:
        backend_dirs.append(data_root / "qemu")
        backend_dirs.append(data_root / "virtualbox")

    for instance_root in [data_root, *backend_dirs]:
        vault_dir = instance_root / "vault"
        instance_records_file = vault_dir / "multipassd-instance-image-records.json"
        instances_dir = vault_dir / "instances"

        print(f"{instances_dir.resolve()}")

        try:
            shutil.rmtree(instances_dir.resolve())
        except Exception as e:
            print(f"Remove error :{e}")
        # Opening via w might override the permissions. Doing it via r+ preserves
        # the existing permissions.
        with contextlib.suppress(FileNotFoundError):
            with open(instance_records_file, "r+", encoding="utf-8") as f:
                f.truncate(0)
                print(f"truncated {instance_records_file}")
