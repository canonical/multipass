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

"""A VM's data and identity -- and a suspended VM -- should survive an upgrade."""

import pytest

from cli.config import cfg
from cli.multipass import multipass, state
from .helpers import (
    make_sentinel,
    write_sentinel,
    assert_sentinel,
    info_fingerprint,
    assert_fingerprint_unchanged,
    resume_seeded,
)
from .seedutils import seeded_vm

LIFECYCLE_VM = "upg-lifecycle"
SUSPEND_VM = "upg-suspend"

#: suspend/resume is not preserved across an upgrade on these drivers.
requires_suspend = pytest.mark.skipif(
    cfg.driver in ("lxd", "applevz"),
    reason=f"suspend/resume is not supported on the `{cfg.driver}` driver",
)


# --- a stopped VM, its data and identity ---------------------------------------

@pytest.mark.seed
def test_lifecycle_seed(seed_manifest):
    with seeded_vm(LIFECYCLE_VM):
        content = make_sentinel("lifecycle")
        path = write_sentinel(LIFECYCLE_VM, "data", content)
        fingerprint = info_fingerprint(LIFECYCLE_VM)  # captured while running
        assert multipass("stop", LIFECYCLE_VM)
        assert state(LIFECYCLE_VM) == "Stopped"

    seed_manifest[LIFECYCLE_VM] = {
        "sentinel_path": path,
        "sentinel_content": content,
        "fingerprint": fingerprint,
    }


@pytest.mark.verify
@pytest.mark.purge(LIFECYCLE_VM)
def test_lifecycle_verify(verify_manifest):
    recorded = verify_manifest[LIFECYCLE_VM]

    resume_seeded(LIFECYCLE_VM, expected_state="Stopped")
    # cpu/memory are only reported while running, so check after the start.
    assert_fingerprint_unchanged(LIFECYCLE_VM, recorded["fingerprint"])
    assert_sentinel(LIFECYCLE_VM, recorded["sentinel_path"], recorded["sentinel_content"])


# --- a suspended VM ------------------------------------------------------------

@pytest.mark.seed
@requires_suspend
def test_suspend_resume_seed(seed_manifest):
    with seeded_vm(SUSPEND_VM):
        content = make_sentinel("suspend")
        path = write_sentinel(SUSPEND_VM, "data", content)
        assert multipass("suspend", SUSPEND_VM)
        assert state(SUSPEND_VM) == "Suspended"

    seed_manifest[SUSPEND_VM] = {
        "sentinel_path": path,
        "sentinel_content": content,
    }


@pytest.mark.verify
@requires_suspend
@pytest.mark.purge(SUSPEND_VM)
def test_suspend_resume_verify(verify_manifest):
    recorded = verify_manifest[SUSPEND_VM]

    # The VM must come back still Suspended, then resume cleanly with its data.
    resume_seeded(SUSPEND_VM, expected_state="Suspended")
    assert_sentinel(SUSPEND_VM, recorded["sentinel_path"], recorded["sentinel_content"])
