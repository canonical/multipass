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
launched via ``seeded_vm`` -- a thin wrapper over the shared ``launch`` helper
that pins the name and sets ``autodelete=False``. These utilities cover the bits
``launch`` does not: making seeding idempotent, and providing a host mount
source that both the daemon can read and the upgrade preserves.
"""

import logging
import shutil
import tempfile
from contextlib import contextmanager
from pathlib import Path

from cli.controller import get_temp_root
from cli.multipass import launch, multipass, vm_exists


def ensure_absent(name: str) -> None:
    """Remove ``name`` if it exists so seeding is idempotent.

    A seed run may be retried on the same host; leftover VMs from a previous
    attempt would otherwise collide with the deterministic names (and trip
    ``launch``'s "must not already exist" precondition).
    """
    if not vm_exists(name):
        return
    logging.info("upgrade-seed :: purging pre-existing `%s`", name)
    assert multipass("delete", name, "--purge"), f"Failed to purge pre-existing `{name}`"
    assert not vm_exists(name), f"Pre-existing `{name}` still exists after purge"


@contextmanager
def seeded_vm(name: str, **launch_overrides):
    """Launch a persistent VM to seed, yielding the cli ``launch`` handle.

    Wraps the shared ``launch`` helper with the two conventions every seed test
    needs: make the name idempotent (``ensure_absent``) and keep the VM around
    after the test process exits (``autodelete=False``) so it survives the
    upgrade. Keyword arguments override the shared launch defaults (e.g.
    ``cpus``, ``memory``, ``disk``, or ``extra_args`` such as ``--network`` or
    ``--cloud-init``).
    """
    ensure_absent(name)
    override = {"name": name, "autodelete": False, **launch_overrides}
    with launch(cfg_override=override) as vm:
        yield vm


def daemon_readable_dir(label: str) -> Path:
    """A fresh, deterministic host directory that Multipass (client and daemon)
    can read, and that survives the package upgrade.

    A strictly-confined snap cannot see the host's ``/tmp`` -- it has a private
    one -- so anything Multipass must read (a mount source, a ``--cloud-init``
    file) has to live under the daemon controller's ``get_temp_root()`` (the
    same place the suite's ``TempDirectory`` uses): ``$SNAP_COMMON/tmp`` for the
    snap, which ``snap refresh`` also preserves. For other controllers a
    deterministic path under the system temp directory is used; it persists on
    the same host between the two runs. The directory is reset on each call so
    re-seeding is idempotent.
    """
    dir_name = f"upgrade-{label}"
    root = Path(get_temp_root() or tempfile.gettempdir())
    path = (root / dir_name).resolve()
    # Reset contents for idempotency across re-seeds.
    if path.exists():
        shutil.rmtree(path, ignore_errors=True)
    path.mkdir(parents=True, exist_ok=True)
    return path
