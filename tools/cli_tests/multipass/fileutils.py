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

def _nuke_vbox_frontends():
    if sys.platform != "win32":
        return
    # We cannot remove the instances folder while the instances are in use.
    subprocess.run(["taskkill", "/IM", "VBoxHeadless.exe", "/F"], check=False)

def nuke_all_instances(data_dir, driver):
    """Remove the instances directory and clean all instance records at the
    specified data root."""

    data_dir = Path(data_dir)

    backend_dirs = [data_dir / "qemu", data_dir / "virtualbox"]

    if driver == "virtualbox":
        _nuke_vbox_frontends()

    for instance_root in [data_dir, *backend_dirs]:
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
