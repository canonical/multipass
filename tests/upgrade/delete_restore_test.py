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

"""A deleted-but-not-purged VM should recover with its data after an upgrade."""

import pytest

from cli.multipass import multipass, state
from .helpers import seed_sentinel, assert_sentinel_record, resume_seeded
from .seedutils import seeded_vm

VM = "upg-delete"


@pytest.mark.seed
@pytest.mark.scenario(VM)
def test_delete_restore_seed(scenario):
    with seeded_vm(VM):
        sentinel = seed_sentinel(VM, "delete")
        assert multipass("delete", VM)
        assert state(VM) == "Deleted"

    scenario.record.update({"sentinel": sentinel})


@pytest.mark.verify
@pytest.mark.scenario(VM)
def test_delete_restore_verify(scenario):
    recorded = scenario.record

    assert state(VM) == "Deleted"
    assert multipass("recover", VM)
    resume_seeded(VM)
    assert_sentinel_record(VM, recorded["sentinel"])
