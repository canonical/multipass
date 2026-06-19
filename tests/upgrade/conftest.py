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

"""Conftest for the version upgrade suite.

The suite lives at ``tests/upgrade`` (a sibling of ``tests/cli``) and is its own
entry point, selected by marker:

    pytest tests/upgrade -m seed       # provision VMs on the installed version
    pytest tests/upgrade -m verify     # validate them after the package upgrade

It reuses the entire cli framework via ``pytest_plugins`` (the daemon
controllers, the ``multipassd`` governor fixture, the sudo/binary session setup,
and the shared ``--driver`` / ``--daemon-controller`` / ``--vm-*`` options). The
``cli`` package is importable because pytest puts ``tests/`` on ``sys.path``
(both ``tests/cli`` and ``tests/upgrade`` are packages; ``tests`` is not).

The manifest -- a plain JSON document -- is the only thing shared between the
two runs. The seed tests fill in ``seed_manifest`` (persisted at session end);
the verify tests read ``verify_manifest``.

Note: because ``pytest_plugins`` must sit in a top-level conftest, run this
suite via its own path (``pytest tests/upgrade``), not as part of a combined
``pytest tests`` invocation.
"""

import json
import logging
from pathlib import Path
from types import SimpleNamespace

import pytest

from cli.config import cfg
from cli.multipass import get_multipass_version, vm_exists, multipass

# Reuse the whole cli test framework (options, fixtures, governor, session setup).
pytest_plugins = ["cli.conftest"]

SCHEMA = 1

# Establish the namespace at import time so it is always safe to read.
cfg.upgrade = SimpleNamespace(manifest="upgrade-manifest.json")


def pytest_addoption(parser):
    parser.addoption(
        "--upgrade-manifest",
        default="upgrade-manifest.json",
        help="Path to the JSON manifest carrying upgrade expectations between "
        "the seed (`-m seed`) and verify (`-m verify`) runs.",
    )


def pytest_configure(config):
    cfg.upgrade.manifest = config.getoption("--upgrade-manifest")


def _env_meta() -> dict:
    return {
        "version": str(get_multipass_version()),
        "driver": cfg.driver,
        "controller": cfg.daemon_controller,
    }


@pytest.fixture(scope="session")
def seed_manifest():
    """Dict the seed tests fill in; written to disk once at session end.

    Only instantiated when seed tests run (``-m seed``), so the verify run never
    overwrites the manifest.
    """
    document = {"schema": SCHEMA, "seed": _env_meta(), "scenarios": {}}

    yield document["scenarios"]

    path = Path(cfg.upgrade.manifest)
    path.parent.mkdir(parents=True, exist_ok=True)
    tmp = path.with_suffix(path.suffix + ".tmp")
    tmp.write_text(json.dumps(document, indent=2, sort_keys=True), encoding="utf-8")
    tmp.replace(path)
    logging.info(
        "upgrade :: wrote %d record(s) to %s", len(document["scenarios"]), path
    )


@pytest.fixture(scope="session")
def verify_manifest():
    """Load the manifest produced by the seed run; only used by ``-m verify``."""
    path = Path(cfg.upgrade.manifest)
    if not path.exists():
        pytest.fail(
            f"Upgrade manifest not found at `{path}`. Run the seed phase "
            "(`pytest tests/upgrade -m seed`) first."
        )
    document = json.loads(path.read_text(encoding="utf-8"))
    assert document.get("schema") == SCHEMA, (
        f"Unsupported upgrade manifest schema `{document.get('schema')}` "
        f"(this harness speaks `{SCHEMA}`)."
    )
    seeded = document.get("seed", {})
    logging.info(
        "upgrade :: verifying upgrade %s -> %s (driver=%s, controller=%s)",
        seeded.get("version"), get_multipass_version(),
        cfg.driver, cfg.daemon_controller,
    )
    yield document["scenarios"]


@pytest.fixture(autouse=True)
def _purge_marked(request):
    """Purge the VMs named in a test's ``@pytest.mark.purge(...)`` once it finishes.

    Runs in teardown, so the VM is removed even if the test fails -- the natural
    way to keep verify tests clean without an inline delete at every return.
    """
    yield
    marker = request.node.get_closest_marker("purge")
    if not marker:
        return
    for name in marker.args:
        if vm_exists(name):
            multipass("delete", name, "--purge")


@pytest.fixture(scope="session", autouse=True)
def _daemon_running(multipassd_session_scoped):
    """Keep the daemon up for the whole suite, reusing the cli framework's
    session-scoped governor fixture."""
    yield multipassd_session_scoped


@pytest.hookimpl(trylast=True)
def pytest_collection_modifyitems(items):
    """Guard the seed/verify split: a test belongs to one phase, and a single
    pytest invocation must not mix the two (the package upgrade happens between
    them, so they are always separate runs).

    ``trylast`` so this runs after pytest's own ``-m`` deselection: when a phase
    is selected, ``items`` holds only that phase and the guard stays quiet.
    """
    phases = {"seed", "verify"}
    selected = set()
    for item in items:
        marks = phases & {mark.name for mark in item.iter_markers()}
        if len(marks) > 1:
            raise pytest.UsageError(f"{item.nodeid} is both seed and verify")
        selected |= marks

    if len(selected) > 1:
        raise pytest.UsageError(
            "seed and verify tests must be run separately; "
            "select a phase with `-m seed` or `-m verify`."
        )
