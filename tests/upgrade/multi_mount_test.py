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

"""Classic and native mounts should both survive an upgrade."""

from pathlib import Path

import sys

import pytest

from cli.config import cfg
from cli.multipass import multipass, mounts, read_file, write_file, path_exists
from cli.utilities import retry
from .helpers import make_sentinel, park_seeded, resume_seeded
from .seedutils import seeded_vm, daemon_readable_dir

VM = "upg-multimount"

# Native mounts are unavailable/unsuitable for this upgrade check on these drivers,
# and on Windows native (SMB) mounts need an interactive password the harness can't
# supply (cf. the cli suite), so skip the whole multi-mount check there.
requires_native_mount = pytest.mark.skipif(
    cfg.driver in ("lxd", "applevz") or sys.platform == "win32",
    reason=f"native mounts are not supported here "
    f"(driver `{cfg.driver}`, platform `{sys.platform}`)",
)


def _guest_target(source: Path) -> str:
    # Multipass mounts to /home/ubuntu/<source basename> by default.
    return (Path("/home/ubuntu") / source.name).as_posix()


@pytest.mark.seed
@pytest.mark.scenario(VM)
@requires_native_mount
def test_multi_mount_seed(scenario):
    classic_source = daemon_readable_dir(f"{VM}-classic")
    native_source = daemon_readable_dir(f"{VM}-native")
    classic_target = _guest_target(classic_source)
    native_target = _guest_target(native_source)

    classic_host_content = make_sentinel("multimount-classic-host")
    native_host_content = make_sentinel("multimount-native-host")
    (classic_source / "host.txt").write_text(classic_host_content, encoding="utf-8")
    (native_source / "host.txt").write_text(native_host_content, encoding="utf-8")

    with seeded_vm(VM):
        assert multipass("stop", VM)
        assert multipass("mount", "--type", "classic", str(classic_source), VM)
        assert multipass("mount", "--type", "native", str(native_source), VM)
        assert multipass("start", VM)

        assert path_exists(VM, f"{classic_target}/host.txt")
        assert path_exists(VM, f"{native_target}/host.txt")
        assert read_file(VM, f"{classic_target}/host.txt").strip() == classic_host_content
        assert read_file(VM, f"{native_target}/host.txt").strip() == native_host_content

        classic_guest_content = make_sentinel("multimount-classic-guest")
        native_guest_content = make_sentinel("multimount-native-guest")
        assert write_file(VM, f"{classic_target}/guest.txt", classic_guest_content)
        assert write_file(VM, f"{native_target}/guest.txt", native_guest_content)
        assert (classic_source / "guest.txt").read_text(encoding="utf-8").strip() == classic_guest_content
        assert (native_source / "guest.txt").read_text(encoding="utf-8").strip() == native_guest_content

        recorded_mounts = mounts(VM)
        park_seeded(VM)

    scenario.record.update({
        "classic_target": classic_target,
        "native_target": native_target,
        "classic_host_content": classic_host_content,
        "native_host_content": native_host_content,
        "classic_guest_content": classic_guest_content,
        "native_guest_content": native_guest_content,
        "mounts": recorded_mounts,
    })


@pytest.mark.verify
@pytest.mark.scenario(VM)
@requires_native_mount
def test_multi_mount_verify(scenario):
    recorded = scenario.record
    classic_target = recorded["classic_target"]
    native_target = recorded["native_target"]

    # The mount definitions must have survived the upgrade.
    assert mounts(VM) == recorded["mounts"], "mount definitions changed across upgrade"
    assert classic_target in mounts(VM)
    assert native_target in mounts(VM)

    resume_seeded(VM, expected_state="Stopped")

    # The mounts are re-established asynchronously on boot; give them a moment.
    @retry(retries=12, delay=5.0)
    def mounts_are_live():
        return path_exists(VM, f"{classic_target}/host.txt", f"{native_target}/host.txt")

    assert mounts_are_live(), f"mounts not re-established after upgrade: `{classic_target}`, `{native_target}`"
    assert read_file(VM, f"{classic_target}/host.txt").strip() == recorded["classic_host_content"]
    assert read_file(VM, f"{native_target}/host.txt").strip() == recorded["native_host_content"]
    assert path_exists(VM, f"{classic_target}/guest.txt")
    assert path_exists(VM, f"{native_target}/guest.txt")
    assert read_file(VM, f"{classic_target}/guest.txt").strip() == recorded["classic_guest_content"]
    assert read_file(VM, f"{native_target}/guest.txt").strip() == recorded["native_guest_content"]

    assert multipass("umount", VM)
