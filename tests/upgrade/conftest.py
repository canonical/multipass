#!/usr/bin/env python3

import json
from dataclasses import dataclass
from pathlib import Path
from types import SimpleNamespace

import pytest

from cli.config import cfg
from cli.conftest import multipassd_impl
from cli.multipass import get_multipass_version, multipass, vm_exists

pytest_plugins = ["cli.conftest"]

SCHEMA = 1
cfg.upgrade = SimpleNamespace(manifest="upgrade-manifest.json")


def pytest_addoption(parser):
    parser.addoption(
        "--upgrade-manifest",
        default="upgrade-manifest.json",
        help="Manifest shared by the seed and verify upgrade phases.",
    )


def pytest_configure(config):
    cfg.upgrade.manifest = config.getoption("--upgrade-manifest")


@pytest.fixture(scope="session")
def seed_manifest():
    document = {
        "schema": SCHEMA,
        "seed": {
            "version": str(get_multipass_version()),
            "driver": cfg.driver,
            "controller": cfg.daemon_controller,
        },
        "scenarios": {},
    }
    yield document["scenarios"]

    path = Path(cfg.upgrade.manifest)
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(json.dumps(document, indent=2, sort_keys=True), encoding="utf-8")
    temporary.replace(path)


@pytest.fixture(scope="session")
def verify_manifest():
    path = Path(cfg.upgrade.manifest)
    if not path.exists():
        pytest.fail(f"Upgrade manifest not found: {path}")

    document = json.loads(path.read_text(encoding="utf-8"))
    assert document.get("schema") == SCHEMA
    seeded = document["seed"]
    assert seeded["driver"] == cfg.driver
    assert seeded["controller"] == cfg.daemon_controller
    return document["scenarios"]


@dataclass
class Scenario:
    vm: str
    record: dict


def scenario_name(request):
    marker = request.node.get_closest_marker("scenario")
    if marker is None or not marker.args:
        raise pytest.UsageError(f"{request.node.nodeid} requires @pytest.mark.scenario")
    return marker.args[0]


@pytest.fixture
def scenario(request):
    name = scenario_name(request)
    if request.node.get_closest_marker("seed"):
        records = request.getfixturevalue("seed_manifest")
        yield Scenario(name, records.setdefault(name, {}))
        return
    if request.node.get_closest_marker("verify"):
        records = request.getfixturevalue("verify_manifest")
        if name not in records:
            pytest.skip(f"Scenario {name} was not seeded")

        yield Scenario(name, records[name])
        if vm_exists(name):
            assert multipass("delete", name, "--purge")
        return

    raise pytest.UsageError(f"{request.node.nodeid} is neither seed nor verify")


@pytest.fixture(scope="session", autouse=True)
def daemon_session(environment_setup):
    with multipassd_impl() as daemon:
        yield daemon


@pytest.hookimpl(trylast=True)
def pytest_collection_modifyitems(config, items):
    if config.option.collectonly:
        return

    phases = set()
    for item in items:
        item_phases = {mark.name for mark in item.iter_markers()} & {"seed", "verify"}
        if len(item_phases) > 1:
            raise pytest.UsageError(f"{item.nodeid} is both seed and verify")
        phases |= item_phases

    if len(phases) > 1:
        raise pytest.UsageError("Select exactly one upgrade phase with -m seed or -m verify")
