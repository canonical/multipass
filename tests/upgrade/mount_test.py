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

"""A classic mount, and the data flowing through it, should survive an upgrade.

The mount definition lives in the instance record and is re-established when the
VM next starts, so after an upgrade we expect: the mount still listed; host data
still visible in the guest; and guest-written data still on the host and
re-exposed. A classic (SSHFS) mount is used so it works without the native-mount
restrictions."""

import sys
from pathlib import Path

import pytest

from cli.multipass import multipass, mounts, read_file, write_file, path_exists
from cli.utilities import retry
from .helpers import make_sentinel, park_seeded, resume_seeded
from .seedutils import seeded_vm, daemon_readable_dir

VM = "upg-mount"


def _guest_target(source: Path) -> str:
    # Multipass mounts to /home/ubuntu/<source basename> by default.
    return (Path("/home/ubuntu") / source.name).as_posix()


@pytest.mark.seed
@pytest.mark.scenario(VM)
def test_mount_seed(seed_scenario):
    source = daemon_readable_dir(VM)
    target = _guest_target(source)

    host_content = make_sentinel("mount-host")
    (source / "host.txt").write_text(host_content, encoding="utf-8")

    with seeded_vm(VM):
        if sys.platform == "win32":
            multipass("set", "local.privileged-mounts=1")

        assert multipass("mount", "--type", "classic", str(source), VM), (
            f"failed to mount `{source}` into `{VM}`"
        )

        # Host -> guest payload visible...
        assert path_exists(VM, f"{target}/host.txt")
        assert read_file(VM, f"{target}/host.txt").strip() == host_content

        # ...and a guest-written file lands on the host.
        guest_content = make_sentinel("mount-guest")
        assert write_file(VM, f"{target}/guest.txt", guest_content)
        assert (source / "guest.txt").read_text(encoding="utf-8").strip() == guest_content

        recorded_mounts = mounts(VM)
        park_seeded(VM)

    seed_scenario.record.update({
        "target": target,
        "host_content": host_content,
        "guest_content": guest_content,
        "mounts": recorded_mounts,
    })


@pytest.mark.verify
@pytest.mark.scenario(VM)
def test_mount_verify(verify_scenario):
    recorded = verify_scenario.record
    target = recorded["target"]

    # The mount definition must have survived the upgrade.
    assert mounts(VM) == recorded["mounts"], "mount definition changed across upgrade"

    resume_seeded(VM, expected_state="Stopped")

    # The mount is re-established asynchronously on boot; give it a moment.
    @retry(retries=12, delay=5.0)
    def mount_is_live():
        return path_exists(VM, f"{target}/host.txt")

    assert mount_is_live(), f"mount `{target}` not re-established after upgrade"
    assert read_file(VM, f"{target}/host.txt").strip() == recorded["host_content"]
    assert path_exists(VM, f"{target}/guest.txt")
    assert read_file(VM, f"{target}/guest.txt").strip() == recorded["guest_content"]

    assert multipass("umount", VM)
