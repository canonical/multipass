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

"""An extra (``--network``) interface should survive an upgrade.

What must survive the upgrade is the *instance's* NIC configuration -- the extra
interface and its persisted MAC, stored in the instance record -- not the host
bridge itself (host networking is expected to be rebuilt, and the user asked
that it not survive a reboot). So the ephemeral host bridge is created and
destroyed *within each phase* by a fixture: seed attaches a VM to it and records
the MACs; verify re-creates the bridge, starts the VM, and confirms the same
MACs came back. The bridge therefore never lingers between runs to perturb other
tests or `multipass networks`. ``mode=manual`` keeps the guest from trying to
DHCP on the isolated bridge.

Linux only: the bridge is created with ``ip link``. The skip applies to both
phases, so seed and verify stay consistent.
"""

import sys

import pytest

from cli.multipass import launch, multipass, state
from .helpers import guest_interface_macs, resume_seeded
from .seedutils import ensure_absent
from .netutils import create_ephemeral_bridge, delete_ephemeral_bridge

VM = "upg-network"
BRIDGE = "mpupgtbr0"

pytestmark = pytest.mark.skipif(
    sys.platform != "linux", reason="ephemeral bridge networking is Linux-only"
)


@pytest.fixture
def ephemeral_bridge():
    """Create the runtime-only host bridge for one test; remove it afterwards.

    Used by both phases, so the bridge exists only for the duration of a single
    test and is gone before any other test (or a later run) sees it.
    """
    create_ephemeral_bridge(BRIDGE)
    yield BRIDGE
    delete_ephemeral_bridge(BRIDGE)


@pytest.mark.seed
@pytest.mark.usefixtures("ephemeral_bridge")
def test_network_seed(seed_manifest):
    ensure_absent(VM)
    with launch(cfg_override={
        "name": VM,
        "autopurge": False,
        "extra_args": ["--network", f"name={BRIDGE},mode=manual"],
    }):
        macs = guest_interface_macs(VM)
        assert len(macs) >= 2, f"expected an extra interface, found MACs: {macs}"
        assert multipass("stop", VM)
        assert state(VM) == "Stopped"

    seed_manifest[VM] = {"macs": macs, "bridge": BRIDGE}


@pytest.mark.verify
@pytest.mark.purge(VM)
@pytest.mark.usefixtures("ephemeral_bridge")
def test_network_verify(verify_manifest):
    recorded = verify_manifest[VM]

    # The bridge is back (fixture), so the stored NIC config can re-attach.
    resume_seeded(VM, expected_state="Stopped")
    assert guest_interface_macs(VM) == recorded["macs"], (
        "guest interface MACs changed across upgrade"
    )
