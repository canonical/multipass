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

"""A mount's uid/gid mappings should survive an upgrade.

Classic (SSHFS) mounts honour mappings on every driver; native mounts honour a
single mapping only on qemu, so the native pair is qemu-only."""

import sys
from pathlib import Path

import pytest

from cli.config import cfg
from cli.multipass import (
    multipass, mounts, read_file, write_file, path_exists,
    exec as guest_exec, default_mount_uid, default_mount_gid,
)
from cli.utilities import retry
from .helpers import make_sentinel, park_seeded, resume_seeded
from .seedutils import seeded_vm, daemon_readable_dir

CLASSIC_VM = "upg-mapmount"
NATIVE_VM = "upg-mapmount-native"
# Map the host id onto guest root so the translation is observable (not identity).
UID_MAP = f"{default_mount_uid()}:0"
GID_MAP = f"{default_mount_gid()}:0"
GUEST_OWNER = "0:0"


def guest_owner(vm, path):
    return guest_exec(vm, "stat", "-c", "%u:%g", path).content.strip()

# Native mappings are qemu-only and capped at one each (qemu_mount_handler).
qemu_native_only = pytest.mark.skipif(
    cfg.driver != "qemu",
    reason=f"native mounts honour mappings only on qemu, not `{cfg.driver}`",
)


def _guest_target(source: Path) -> str:
    # Multipass mounts to /home/ubuntu/<source basename> by default.
    return (Path("/home/ubuntu") / source.name).as_posix()


def _seed(vm, mount_type, label, record):
    source = daemon_readable_dir(vm)
    target = _guest_target(source)
    host_content = make_sentinel(f"{label}-host")
    (source / "host.txt").write_text(host_content, encoding="utf-8")

    with seeded_vm(vm):
        if sys.platform == "win32":
            assert multipass("set", "local.privileged-mounts=1")

        # Native mounts attach to a stopped instance.
        assert multipass("stop", vm)
        assert multipass(
            "mount", "--type", mount_type,
            "--uid-map", UID_MAP, "--gid-map", GID_MAP, str(source), vm,
        ), f"failed to mount `{source}` into `{vm}`"
        assert multipass("start", vm)

        assert path_exists(vm, f"{target}/host.txt")
        assert read_file(vm, f"{target}/host.txt").strip() == host_content
        # The host id was mapped onto guest root: the file must show as 0:0.
        if sys.platform != "win32":
            assert guest_owner(vm, f"{target}/host.txt") == GUEST_OWNER, (
                f"mapping not applied: `{target}/host.txt` owner mismatch"
            )

        guest_content = None
        # Native (qemu 9p, security_model=passthrough) maps a single uid/gid pair
        # one-to-one with no reverse mapping, so a guest-side write by ubuntu (1000)
        # has no host id to land on and 9p denies it. Classic (sshfs) rewrites ids
        # both ways, so guest writeback works there. Host->guest ownership above is
        # the mapping check common to both; writeback is classic-only.
        if mount_type == "classic":
            guest_content = make_sentinel(f"{label}-guest")
            assert write_file(vm, f"{target}/guest.txt", guest_content)
            assert (source / "guest.txt").read_text(encoding="utf-8").strip() == guest_content

        recorded_mounts = mounts(vm)
        assert recorded_mounts[target]["uid_mappings"] == [UID_MAP]
        assert recorded_mounts[target]["gid_mappings"] == [GID_MAP]
        park_seeded(vm)

    record.update({
        "target": target,
        "host_content": host_content,
        "guest_content": guest_content,
        "mounts": recorded_mounts,
    })


def _verify(vm, record):
    target = record["target"]
    assert mounts(vm) == record["mounts"], "mount definition changed across upgrade"
    assert mounts(vm)[target]["uid_mappings"] == [UID_MAP]
    assert mounts(vm)[target]["gid_mappings"] == [GID_MAP]

    resume_seeded(vm, expected_state="Stopped")

    @retry(retries=12, delay=5.0)
    def mount_is_live():
        return path_exists(vm, f"{target}/host.txt")

    assert mount_is_live(), f"mount `{target}` not re-established after upgrade"
    assert read_file(vm, f"{target}/host.txt").strip() == record["host_content"]
    # The mapping must still translate host id -> guest root after the upgrade.
    if sys.platform != "win32":
        assert guest_owner(vm, f"{target}/host.txt") == GUEST_OWNER, (
            "mapping no longer applied after upgrade"
        )
    if record["guest_content"] is not None:
        assert path_exists(vm, f"{target}/guest.txt")
        assert read_file(vm, f"{target}/guest.txt").strip() == record["guest_content"]
    assert multipass("umount", vm)


@pytest.mark.seed
@pytest.mark.scenario(CLASSIC_VM)
def test_mount_mappings_seed(scenario):
    _seed(CLASSIC_VM, "classic", "mapmount", scenario.record)


@pytest.mark.verify
@pytest.mark.scenario(CLASSIC_VM)
def test_mount_mappings_verify(scenario):
    _verify(CLASSIC_VM, scenario.record)


@pytest.mark.seed
@pytest.mark.scenario(NATIVE_VM)
@qemu_native_only
def test_native_mount_mappings_seed(scenario):
    _seed(NATIVE_VM, "native", "mapmount-native", scenario.record)


@pytest.mark.verify
@pytest.mark.scenario(NATIVE_VM)
@qemu_native_only
def test_native_mount_mappings_verify(scenario):
    _verify(NATIVE_VM, scenario.record)
