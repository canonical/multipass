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

"""A stopped VM's data and identity should survive an upgrade."""

import pytest

from .helpers import (
    seed_sentinel,
    assert_sentinel_record,
    info_fingerprint,
    assert_fingerprint_unchanged,
    park_seeded,
    resume_seeded,
)
from .seedutils import seeded_vm

LIFECYCLE_VM = "upg-lifecycle"


@pytest.mark.seed
@pytest.mark.scenario(LIFECYCLE_VM)
def test_lifecycle_seed(seed_scenario):
    with seeded_vm(LIFECYCLE_VM):
        sentinel = seed_sentinel(LIFECYCLE_VM, "lifecycle")
        fingerprint = info_fingerprint(LIFECYCLE_VM)  # captured while running
        park_seeded(LIFECYCLE_VM)

    seed_scenario.record.update({"sentinel": sentinel, "fingerprint": fingerprint})


@pytest.mark.verify
@pytest.mark.scenario(LIFECYCLE_VM)
def test_lifecycle_verify(verify_scenario):
    recorded = verify_scenario.record

    resume_seeded(LIFECYCLE_VM, expected_state="Stopped")
    # cpu/memory are only reported while running, so check after the start.
    assert_fingerprint_unchanged(LIFECYCLE_VM, recorded["fingerprint"])
    assert_sentinel_record(LIFECYCLE_VM, recorded["sentinel"])
