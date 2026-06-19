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

"""A snapshot tree -- its metadata and the data captured in it -- should survive
an upgrade. This is the case most sensitive to on-disk format migrations."""

import pytest

from cli.multipass import multipass, state, snapshot_count, path_exists
from .helpers import make_sentinel, write_sentinel
from .seedutils import seeded_vm

VM = "upg-snapshot"
BASE = "snap-base"
CHILD = "snap-child"
BASE_COMMENT = "upgrade base snapshot"
CHILD_COMMENT = "upgrade child snapshot"


def _snapshot(name, snapshot_name, comment):
    assert multipass("stop", name)
    with multipass("snapshot", name, "--name", snapshot_name, "--comment", comment) as out:
        assert out, f"Failed to snapshot `{name}` as `{snapshot_name}`: {out}"


@pytest.mark.seed
@pytest.mark.snapshot
@pytest.mark.scenario(VM)
def test_snapshot_seed(seed_scenario):
    with seeded_vm(VM):
        # Data captured by BASE only.
        base_only = make_sentinel("snapshot-base")
        base_only_path = write_sentinel(VM, "base-only", base_only)
        _snapshot(VM, BASE, BASE_COMMENT)

        # Data added after BASE -- present in CHILD, gone if we roll back to BASE.
        assert multipass("start", VM)
        child_only = make_sentinel("snapshot-child")
        child_only_path = write_sentinel(VM, "child-only", child_only)
        _snapshot(VM, CHILD, CHILD_COMMENT)
        assert state(VM) == "Stopped"

    seed_scenario.record.update({
        "snapshot_count": snapshot_count(VM),
        "snapshots": {
            BASE: {"parent": "", "comment": BASE_COMMENT},
            CHILD: {"parent": BASE, "comment": CHILD_COMMENT},
        },
        "base_only_path": base_only_path,
        "child_only_path": child_only_path,
    })


def _list_snapshots(name):
    with multipass("list", "--format=json", "--snapshots").json() as out:
        assert out, f"snapshot listing failed: {out}"
        return out["info"].get(name, {})


@pytest.mark.verify
@pytest.mark.snapshot
@pytest.mark.scenario(VM)
def test_snapshot_verify(verify_scenario):
    recorded = verify_scenario.record

    assert state(VM) == "Stopped"
    assert snapshot_count(VM) == recorded["snapshot_count"], "snapshot count changed"

    # Metadata: every snapshot still present with the right parent and comment.
    present = _list_snapshots(VM)
    for snap, meta in recorded["snapshots"].items():
        assert snap in present, f"snapshot `{snap}` vanished across upgrade"
        assert present[snap]["parent"] == meta["parent"], f"parent of `{snap}` changed"
        assert present[snap]["comment"] == meta["comment"], f"comment of `{snap}` changed"

    # Contents: restoring BASE reveals base-only data and hides child-only data.
    assert multipass("restore", f"{VM}.{BASE}", "--destructive")
    assert multipass("start", VM)
    assert path_exists(VM, recorded["base_only_path"]), "base-only data lost after restore"
    assert not path_exists(VM, recorded["child_only_path"]), "child-only data leaked into base"
