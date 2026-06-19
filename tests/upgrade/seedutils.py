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

"""Helpers used while *seeding* upgrade VMs.

Seeded VMs must outlive the test process and the package upgrade, so they are
launched via the shared ``launch`` helper with ``autopurge=False``. These
utilities cover the bits ``launch`` does not: making seeding idempotent, and
providing a host mount source that both the daemon can read and the upgrade
preserves.
"""

import logging
import shutil
import tempfile
from pathlib import Path

from cli.config import cfg
from cli.multipass import multipass, vm_exists
from cli.utilities import run_in_new_interpreter
from cli.utilities.temputils import get_snap_temp_root, make_test_tmp_dir_for_snap


def ensure_absent(name: str) -> None:
    """Remove ``name`` if it exists so seeding is idempotent.

    A seed run may be retried on the same host; leftover VMs from a previous
    attempt would otherwise collide with the deterministic names (and trip
    ``launch``'s "must not already exist" precondition).
    """
    if not vm_exists(name):
        return
    logging.info("upgrade-seed :: purging pre-existing `%s`", name)
    multipass("delete", name, "--purge")


def daemon_readable_dir(label: str) -> Path:
    """A fresh, deterministic host directory that Multipass (client and daemon)
    can read, and that survives the package upgrade.

    A strictly-confined snap cannot see the host's ``/tmp`` -- it has a private
    one -- so anything Multipass must read (a mount source, a ``--cloud-init``
    file) has to live somewhere inside the snap's world. For the snap this is
    ``$SNAP_COMMON/tmp`` (the same place the suite's ``TempDirectory`` uses),
    which ``snap refresh`` also preserves. For other controllers a deterministic
    path under the system temp directory is used; it persists on the same host
    between the two runs. The directory is reset on each call so re-seeding is
    idempotent.
    """
    dir_name = f"upgrade-{label}"
    if cfg.daemon_controller == "snap":
        run_in_new_interpreter(make_test_tmp_dir_for_snap, "multipass", privileged=True)
        root = Path(get_snap_temp_root("multipass"))
    else:
        root = Path(tempfile.gettempdir())
    path = (root / dir_name).resolve()
    # Reset contents for idempotency across re-seeds.
    if path.exists():
        shutil.rmtree(path, ignore_errors=True)
    path.mkdir(parents=True, exist_ok=True)
    return path
