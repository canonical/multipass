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
network itself. The host network the VM attaches to is provided per-platform by
the ``host_network`` fixture (see ``netutils``): a throwaway isolated bridge on
Linux, an existing host network (Hyper-V switch / interface) on Windows/macOS.
Seed attaches a VM and records the MACs; verify makes sure the same host network
is present, starts the VM, and confirms the same MACs came back. ``mode=manual``
keeps the guest from trying to DHCP on it.
"""

import pytest

from .helpers import guest_interface_macs, park_seeded, resume_seeded
from .seedutils import seeded_vm
from .netutils import upgrade_network

VM = "upg-network"


@pytest.fixture
def host_network():
    """A host network to attach to, for the duration of one test.

    On Linux it is a freshly-created isolated bridge, removed afterwards; on
    Windows/macOS it is an existing host network that simply persists. Requested
    by both phases so the network is in place whenever the VM is started.
    """
    with upgrade_network() as name:
        yield name


@pytest.mark.seed
@pytest.mark.scenario(VM)
def test_network_seed(seed_scenario, host_network):
    with seeded_vm(VM, extra_args=["--network", f"name={host_network},mode=manual"]):
        macs = guest_interface_macs(VM)
        assert len(macs) >= 2, f"expected an extra interface, found MACs: {macs}"
        park_seeded(VM)

    seed_scenario.record.update({"macs": macs, "network": host_network})


@pytest.mark.verify
@pytest.mark.scenario(VM)
def test_network_verify(verify_scenario, host_network):
    recorded = verify_scenario.record

    # The host network is back (fixture), so the stored NIC config can re-attach.
    resume_seeded(VM, expected_state="Stopped")
    assert guest_interface_macs(VM) == recorded["macs"], (
        "guest interface MACs changed across upgrade"
    )
