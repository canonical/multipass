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

"""Unit tests for pairing the upgrade suite's verify run with its seed run."""

import json
from types import SimpleNamespace

import pytest

from cli.config import cfg
from upgrade import conftest as upgrade


class FakeItem:
    def __init__(self, scenario=None):
        self.marks = {}
        if scenario is not None:
            self.marks["scenario"] = SimpleNamespace(args=(scenario,))

    def get_closest_marker(self, name):
        return self.marks.get(name)


@pytest.fixture
def manifest(tmp_path, monkeypatch):
    path = tmp_path / "manifest.json"
    monkeypatch.setattr(cfg.upgrade, "manifest", str(path), raising=False)
    monkeypatch.setattr(cfg, "driver", "qemu", raising=False)
    monkeypatch.setattr(cfg, "daemon_controller", "snap", raising=False)
    return path


def make_config():
    deselected = []
    hook = SimpleNamespace(pytest_deselected=lambda items: deselected.extend(items))
    return SimpleNamespace(cache=None, hook=hook), deselected


def test_seed_document_records_selected_scenarios(manifest):
    manifest.write_text(json.dumps({
        "schema": upgrade.SCHEMA,
        "scenarios": {"upg-old": {"k": "v"}},
        "selected": ["upg-old", "upg-skipped"],
    }))
    config, _ = make_config()

    document = upgrade._open_seed_document(
        config, [FakeItem("upg-b"), FakeItem("upg-a"), FakeItem()]
    )

    assert document["selected"] == ["upg-a", "upg-b", "upg-old", "upg-skipped"]
    assert document["scenarios"] == {"upg-old": {"k": "v"}}


def test_verify_deselects_scenarios_not_selected_at_seed(manifest):
    manifest.write_text(json.dumps({
        "schema": upgrade.SCHEMA, "scenarios": {}, "selected": ["upg-a"],
    }))
    config, deselected = make_config()
    seeded, other, unnamed = FakeItem("upg-a"), FakeItem("upg-b"), FakeItem()
    items = [seeded, other, unnamed]

    upgrade._deselect_unseeded(config, items)

    assert items == [seeded, unnamed]
    assert deselected == [other]


def test_verify_keeps_all_scenarios_without_selection(manifest):
    manifest.write_text(json.dumps({"schema": upgrade.SCHEMA, "scenarios": {}}))
    config, deselected = make_config()
    items = [FakeItem("upg-a"), FakeItem("upg-b")]

    upgrade._deselect_unseeded(config, list(items))

    assert not deselected
