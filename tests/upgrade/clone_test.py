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

"""A stopped VM should clone cleanly after an upgrade, with data intact."""

import pytest

from cli.multipass import multipass, state, vm_exists
from .helpers import seed_sentinel, assert_sentinel_record, park_seeded, resume_seeded
from .seedutils import seeded_vm

VM = "upg-clone"
CLONE = f"{VM}-clone1"


@pytest.mark.seed
@pytest.mark.scenario(VM)
def test_clone_seed(scenario):
    with seeded_vm(VM):
        sentinel = seed_sentinel(VM, "clone")
        park_seeded(VM)

    scenario.record.update({"sentinel": sentinel})


@pytest.mark.verify
@pytest.mark.scenario(VM)
def test_clone_verify(scenario):
    recorded = scenario.record

    assert state(VM) == "Stopped"
    if vm_exists(CLONE):
        assert multipass("delete", CLONE, "--purge")

    try:
        with multipass("clone", VM) as output:
            assert output
            assert f"Cloned from {VM} to {CLONE}" in output

        assert state(CLONE) == "Stopped"
        resume_seeded(VM)
        assert multipass("start", CLONE)
        assert_sentinel_record(VM, recorded["sentinel"])
        assert_sentinel_record(CLONE, recorded["sentinel"])
    finally:
        if vm_exists(CLONE):
            multipass("delete", CLONE, "--purge")
