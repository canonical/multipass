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

"""A suspended VM should come back Suspended and resume cleanly after an upgrade."""

import pytest

from cli.config import cfg
from .helpers import seed_sentinel, assert_sentinel_record, park_seeded, resume_seeded
from .seedutils import seeded_vm

SUSPEND_VM = "upg-suspend"

#: suspend/resume is not preserved across an upgrade on these drivers. This is
#: broader than the cli framework's ``@pytest.mark.suspend`` (which only gates
#: applevz), so we keep a local skip rather than reuse that marker.
requires_suspend = pytest.mark.skipif(
    cfg.driver in ("lxd", "applevz"),
    reason=f"suspend/resume is not supported on the `{cfg.driver}` driver",
)


@pytest.mark.seed
@pytest.mark.scenario(SUSPEND_VM)
@requires_suspend
def test_suspend_resume_seed(seed_scenario):
    with seeded_vm(SUSPEND_VM):
        sentinel = seed_sentinel(SUSPEND_VM, "suspend")
        park_seeded(SUSPEND_VM, how="suspend", expected="Suspended")

    seed_scenario.record.update({"sentinel": sentinel})


@pytest.mark.verify
@pytest.mark.scenario(SUSPEND_VM)
@requires_suspend
def test_suspend_resume_verify(verify_scenario):
    recorded = verify_scenario.record

    # The VM must come back still Suspended, then resume cleanly with its data.
    resume_seeded(SUSPEND_VM, expected_state="Suspended")
    assert_sentinel_record(SUSPEND_VM, recorded["sentinel"])
