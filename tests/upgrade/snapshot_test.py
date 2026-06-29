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

"""A snapshot tree -- metadata, captured data, and captured resources -- should
survive an upgrade. This is the case most sensitive to on-disk format migrations.

Restoring BASE must reveal base-only data, hide child-only data, and roll cpu /
memory / disk back to what they were when BASE was taken."""

import pytest

from cli.multipass import info, multipass, state, snapshot_count, path_exists
from .helpers import make_sentinel, write_sentinel
from .seedutils import seeded_vm

VM = "upg-snapshot"
BASE = "snap-base"
CHILD = "snap-child"
BASE_COMMENT = "upgrade base snapshot"
CHILD_COMMENT = "upgrade child snapshot"
FORK = "snap-fork"
FORK_COMMENT = "upgrade fork snapshot"


def _snapshot(name, snapshot_name, comment):
    assert multipass("stop", name)
    with multipass("snapshot", name, "--name", snapshot_name, "--comment", comment) as out:
        assert out, f"Failed to snapshot `{name}` as `{snapshot_name}`: {out}"


def _resources(name):
    fields = info(name)[name]
    disks = fields.get("disks", {})
    disk = next(iter(disks.values()), {})
    return {
        "cpu_count": fields.get("cpu_count"),
        "disk_total": disk.get("total"),
        "memory_total": fields.get("memory", {}).get("total"),
    }


@pytest.mark.seed
@pytest.mark.snapshot
@pytest.mark.scenario(VM)
def test_snapshot_seed(scenario):
    with seeded_vm(VM, cpus="2", memory="1G", disk="6G"):
        # Data + resources captured by BASE only.
        base_only = make_sentinel("snapshot-base")
        base_only_path = write_sentinel(VM, "base-only", base_only)
        base_resources = _resources(VM)
        _snapshot(VM, BASE, BASE_COMMENT)

        # Data added and resources bumped after BASE -- present in CHILD, gone on rollback.
        assert multipass("start", VM)
        child_only = make_sentinel("snapshot-child")
        child_only_path = write_sentinel(VM, "child-only", child_only)
        assert multipass("stop", VM)
        assert multipass("set", f"local.{VM}.cpus=3")
        assert multipass("set", f"local.{VM}.memory=2G")
        _snapshot(VM, CHILD, CHILD_COMMENT)
        assert state(VM) == "Stopped"

        # Branch off BASE again -- BASE now has two children. fork-only data lands
        # on a sibling of CHILD, not its descendant.
        assert multipass("restore", f"{VM}.{BASE}", "--destructive")
        assert multipass("start", VM)
        fork_only = make_sentinel("snapshot-fork")
        fork_only_path = write_sentinel(VM, "fork-only", fork_only)
        _snapshot(VM, FORK, FORK_COMMENT)
        assert state(VM) == "Stopped"

    scenario.record.update({
        "snapshot_count": snapshot_count(VM),
        "snapshots": {
            BASE: {"parent": "", "comment": BASE_COMMENT},
            CHILD: {"parent": BASE, "comment": CHILD_COMMENT},
            FORK: {"parent": BASE, "comment": FORK_COMMENT},
        },
        "base_only_path": base_only_path,
        "child_only_path": child_only_path,
        "fork_only_path": fork_only_path,
        "base_resources": base_resources,
    })


def _list_snapshots(name):
    with multipass("list", "--format=json", "--snapshots").json() as out:
        assert out, f"snapshot listing failed: {out}"
        return out["info"].get(name, {})


@pytest.mark.verify
@pytest.mark.snapshot
@pytest.mark.scenario(VM)
def test_snapshot_verify(scenario):
    recorded = scenario.record

    assert state(VM) == "Stopped"
    assert snapshot_count(VM) == recorded["snapshot_count"], "snapshot count changed"

    # Metadata: every snapshot still present with the right parent and comment.
    present = _list_snapshots(VM)
    for snap, meta in recorded["snapshots"].items():
        assert snap in present, f"snapshot `{snap}` vanished across upgrade"
        assert present[snap]["parent"] == meta["parent"], f"parent of `{snap}` changed"
        assert present[snap]["comment"] == meta["comment"], f"comment of `{snap}` changed"

    # Contents + resources: restoring BASE reveals base-only data, hides child-only
    # data, and rolls cpu/memory back to the BASE values.
    assert multipass("restore", f"{VM}.{BASE}", "--destructive")
    assert multipass("start", VM)
    assert path_exists(VM, recorded["base_only_path"]), "base-only data lost after restore"
    assert not path_exists(VM, recorded["child_only_path"]), "child-only data leaked into base"
    assert _resources(VM) == recorded["base_resources"], "resources not restored to BASE"

    # FORK is a sibling of CHILD: restoring it shows fork data, not child data.
    assert multipass("stop", VM)
    assert multipass("restore", f"{VM}.{FORK}", "--destructive")
    assert multipass("start", VM)
    assert path_exists(VM, recorded["fork_only_path"]), "fork-only data lost after restore"
    assert not path_exists(VM, recorded["child_only_path"]), "child-only data leaked into fork"

    # CHILD descends from BASE: its base + child data are present, fork data is not.
    assert multipass("stop", VM)
    assert multipass("restore", f"{VM}.{CHILD}", "--destructive")
    assert multipass("start", VM)
    assert path_exists(VM, recorded["base_only_path"]), "base-only data lost in child"
    assert path_exists(VM, recorded["child_only_path"]), "child-only data lost after restore"
    assert not path_exists(VM, recorded["fork_only_path"]), "fork-only data leaked into child"
