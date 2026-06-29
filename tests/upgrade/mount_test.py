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

"""A mount, and the data flowing through it, should survive an upgrade.

The mount definition lives in the instance record and is re-established when the
VM next starts, so after an upgrade we expect: the mount still listed; host data
still visible in the guest; and guest-written data still on the host and
re-exposed. Covered for classic (SSHFS) and native (qemu 9p) mounts on a stopped
VM, and for a classic mount on a suspended VM; native/suspend pairs skip where
unsupported."""

from pathlib import Path

import pytest

from cli.config import cfg
from cli.multipass import multipass, mounts, read_file, write_file, path_exists
from cli.utilities import retry
from .helpers import make_sentinel, park_seeded, resume_seeded, enable_privileged_mounts
from .seedutils import seeded_vm, daemon_readable_dir

CLASSIC_VM = "upg-mount"
NATIVE_VM = "upg-mount-native"
SUSPEND_VM = "upg-suspmount"

unsupported = pytest.mark.skipif(
    cfg.driver in ("lxd", "applevz"),
    reason=f"native mounts and suspend are not supported on the `{cfg.driver}` driver",
)


def _guest_target(source: Path) -> str:
    # Multipass mounts to /home/ubuntu/<source basename> by default.
    return (Path("/home/ubuntu") / source.name).as_posix()


def _seed(vm, mount_type, label, record, governor, park="stop", expected="Stopped"):
    source = daemon_readable_dir(vm)
    target = _guest_target(source)
    host_content = make_sentinel(f"{label}-host")
    (source / "host.txt").write_text(host_content, encoding="utf-8")

    with seeded_vm(vm):
        enable_privileged_mounts(governor, vm)

        # Native mounts attach to a stopped instance.
        assert multipass("stop", vm)
        assert multipass("mount", "--type", mount_type, str(source), vm), (
            f"failed to mount `{source}` into `{vm}`"
        )
        assert multipass("start", vm)

        assert path_exists(vm, f"{target}/host.txt")
        assert read_file(vm, f"{target}/host.txt").strip() == host_content

        guest_content = make_sentinel(f"{label}-guest")
        assert write_file(vm, f"{target}/guest.txt", guest_content)
        assert (source / "guest.txt").read_text(encoding="utf-8").strip() == guest_content

        recorded_mounts = mounts(vm)
        park_seeded(vm, how=park, expected=expected)

    record.update({
        "target": target,
        "host_content": host_content,
        "guest_content": guest_content,
        "mounts": recorded_mounts,
        "expected": expected,
    })


def _verify(vm, record):
    target = record["target"]
    assert mounts(vm) == record["mounts"], "mount definition changed across upgrade"

    resume_seeded(vm, expected_state=record["expected"])

    @retry(retries=12, delay=5.0)
    def mount_is_live():
        return path_exists(vm, f"{target}/host.txt")

    assert mount_is_live(), f"mount `{target}` not re-established after upgrade"
    assert read_file(vm, f"{target}/host.txt").strip() == record["host_content"]
    assert path_exists(vm, f"{target}/guest.txt")
    assert read_file(vm, f"{target}/guest.txt").strip() == record["guest_content"]
    assert multipass("umount", vm)


@pytest.mark.seed
@pytest.mark.scenario(CLASSIC_VM)
def test_mount_seed(scenario, multipassd_session_scoped):
    _seed(CLASSIC_VM, "classic", "mount", scenario.record, multipassd_session_scoped)


@pytest.mark.verify
@pytest.mark.scenario(CLASSIC_VM)
def test_mount_verify(scenario):
    _verify(CLASSIC_VM, scenario.record)


@pytest.mark.seed
@pytest.mark.scenario(NATIVE_VM)
@unsupported
def test_native_mount_seed(scenario, multipassd_session_scoped):
    _seed(NATIVE_VM, "native", "native-mount", scenario.record, multipassd_session_scoped)


@pytest.mark.verify
@pytest.mark.scenario(NATIVE_VM)
@unsupported
def test_native_mount_verify(scenario):
    _verify(NATIVE_VM, scenario.record)


@pytest.mark.seed
@pytest.mark.scenario(SUSPEND_VM)
@unsupported
def test_suspend_mount_seed(scenario, multipassd_session_scoped):
    _seed(SUSPEND_VM, "classic", "suspmount", scenario.record, multipassd_session_scoped, park="suspend", expected="Suspended")


@pytest.mark.verify
@pytest.mark.scenario(SUSPEND_VM)
@unsupported
def test_suspend_mount_verify(scenario):
    _verify(SUSPEND_VM, scenario.record)
