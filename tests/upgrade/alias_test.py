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

"""Aliases — across contexts, working-dir modes, and counts — survive an upgrade."""

import pytest

from cli.multipass import multipass, vm_exists
from .helpers import park_seeded, resume_seeded
from .seedutils import seeded_vm

VM = "upg-alias"
CONTEXT = "upg-ctx"

# alias name -> (command, working-directory, extra args)
DEFAULT_ALIASES = {"listit": ("ls", "map", [])}
CONTEXT_ALIASES = {
    "wd": ("pwd", "map", []),
    "nomap": ("env", "default", ["--no-map-working-directory"]),
}


def _expected(name, command, wd):
    return {"command": command, "instance": VM, "working-directory": wd}


@pytest.mark.seed
@pytest.mark.scenario(VM)
def test_alias_seed(scenario):
    with seeded_vm(VM):
        assert multipass("prefer", "default")
        for name, (cmd, _wd, extra) in DEFAULT_ALIASES.items():
            assert multipass("alias", f"{VM}:{cmd}", name, *extra)

        assert multipass("prefer", CONTEXT)
        for name, (cmd, _wd, extra) in CONTEXT_ALIASES.items():
            assert multipass("alias", f"{VM}:{cmd}", name, *extra)

        park_seeded(VM)

    # CONTEXT is left active so the verify phase can confirm it persists.
    scenario.record.update({"active_context": CONTEXT})


@pytest.mark.verify
@pytest.mark.scenario(VM)
def test_alias_verify(scenario, request):
    recorded = scenario.record
    request.addfinalizer(lambda: multipass("prefer", "default"))

    assert vm_exists(VM)
    resume_seeded(VM, expected_state="Stopped")

    listing = multipass("aliases", "--format=json").json()
    assert listing["active-context"] == recorded["active_context"], (
        "active alias context changed across upgrade"
    )
    assert listing["contexts"]["default"] == {
        n: _expected(n, c, w) for n, (c, w, _e) in DEFAULT_ALIASES.items()
    }
    assert listing["contexts"][CONTEXT] == {
        n: _expected(n, c, w) for n, (c, w, _e) in CONTEXT_ALIASES.items()
    }

    # An alias from the (active) custom context still runs.
    assert multipass("wd")
