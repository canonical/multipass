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
import contextlib


def nuke_all_instances(data_root):
    """Remove the instances directory and clean all instance records at the
    specified data root."""

    instance_records_file = os.path.join(
        data_root, "data/vault/multipassd-instance-image-records.json"
    )

    instances_dir = os.path.join(data_root, "data/vault/instances")

    shutil.rmtree(instances_dir, ignore_errors=True)

    # Opening via w might override the permissions. Doing it via r+ preserves
    # the existing permissions.
    with contextlib.suppress(FileNotFoundError):
        with open(instance_records_file, "r+", encoding="utf-8") as f:
            f.truncate(0)
