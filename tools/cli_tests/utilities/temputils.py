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

from contextlib import contextmanager, suppress
from pathlib import Path
import tempfile

from cli_tests.config import config
from cli_tests.utilities import run_in_new_interpreter


def get_snap_temp_root(snap_name):
    return f"/var/snap/{snap_name}/common/tmp"


def make_test_tmp_dir_for_snap(snap_name):
    # Make it behave as a real temp dir.
    p = Path(get_snap_temp_root(snap_name))
    p.mkdir(exist_ok=True)
    p.chmod(0o1777)


@contextmanager
def TempDirectory(delete=True, ignore_cleanup_errors=True):
    tmp_root = None
    if config.daemon_controller == "snap":
        run_in_new_interpreter(make_test_tmp_dir_for_snap,
                               "multipass", privileged=True)
        tmp_root = get_snap_temp_root("multipass")
    # Cleanup is best effort.
    with suppress(Exception):
        with tempfile.TemporaryDirectory(dir=tmp_root, delete=delete, ignore_cleanup_errors=ignore_cleanup_errors) as tmp:
            yield Path(tmp).resolve()
