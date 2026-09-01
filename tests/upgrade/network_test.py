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

"""An extra (``--network``) interface should survive an upgrade, parked either
stopped or suspended; a ``--bridged`` interface should too.

What must survive is the *instance's* NIC configuration -- the extra interface
and its persisted MAC, stored in the instance record -- not the host network
itself. The host network is provided per-platform by the ``host_network``
fixture (see ``netutils``): a throwaway isolated bridge on Linux and a private
Hyper-V vSwitch on Windows (when running on a Hyper-V backend). The test is
skipped on macOS where no isolated ephemeral network can be created. Seed
attaches a VM and records the MACs; verify resumes the VM and checks the same
MACs are still reported. ``mode=manual`` keeps the guest from DHCP-ing on it.
The bridged case points ``local.bridged-network`` at the same host network. The
suspend pair is skipped where suspend/resume isn't kept."""

import pytest

from cli.config import cfg
from cli.multipass import multipass
from .helpers import guest_interface_macs, park_seeded, resume_seeded
from .seedutils import seeded_vm
from .netutils import upgrade_network

STOP_VM = "upg-network"
SUSPEND_VM = "upg-net-susp"
BRIDGED_VM = "upg-bridged"

requires_suspend = pytest.mark.skipif(
    cfg.driver in ("lxd", "applevz"),
    reason=f"suspend/resume is not supported on the `{cfg.driver}` driver",
)


@pytest.fixture
def host_network():
    with upgrade_network() as name:
        yield name


def _seed(vm, host_network, record, how="stop", expected="Stopped"):
    with seeded_vm(vm, extra_args=["--network", f"name={host_network},mode=manual"]):
        macs = guest_interface_macs(vm)
        assert len(macs) >= 2, f"expected an extra interface, found MACs: {macs}"
        park_seeded(vm, how=how, expected=expected)
    record.update({"macs": macs, "network": host_network})


def _verify(vm, record, expected="Stopped"):
    resume_seeded(vm, expected_state=expected)
    assert guest_interface_macs(vm) == record["macs"], (
        "guest interface MACs changed across upgrade"
    )


@pytest.mark.seed
@pytest.mark.scenario(STOP_VM)
def test_network_seed(scenario, host_network):
    _seed(STOP_VM, host_network, scenario.record)


@pytest.mark.verify
@pytest.mark.scenario(STOP_VM)
def test_network_verify(scenario, host_network):
    _verify(STOP_VM, scenario.record)


@pytest.mark.seed
@pytest.mark.scenario(SUSPEND_VM)
@requires_suspend
def test_network_suspend_seed(scenario, host_network):
    _seed(SUSPEND_VM, host_network, scenario.record, how="suspend", expected="Suspended")


@pytest.mark.verify
@pytest.mark.scenario(SUSPEND_VM)
@requires_suspend
def test_network_suspend_verify(scenario, host_network):
    _verify(SUSPEND_VM, scenario.record, expected="Suspended")


@pytest.mark.seed
@pytest.mark.scenario(BRIDGED_VM)
def test_bridged_seed(scenario, host_network, request, multipassd_session_scoped):
    # Both setting and clearing local.bridged-network bounce the daemon; each
    # time we must wait for the restart to actually complete (a bare readiness
    # probe races the still-answering, about-to-die daemon and returns early,
    # leaving the next test to hit a dead socket).
    def _reset():
        assert multipass("set", "local.bridged-network=")
        multipassd_session_scoped.wait_for_restart()

    request.addfinalizer(_reset)
    assert multipass("set", f"local.bridged-network={host_network}")
    multipassd_session_scoped.wait_for_restart()

    with seeded_vm(BRIDGED_VM, extra_args=["--bridged"]):
        macs = guest_interface_macs(BRIDGED_VM)
        assert len(macs) >= 2, f"expected a bridged interface, found MACs: {macs}"
        park_seeded(BRIDGED_VM)
    scenario.record.update({"macs": macs, "network": host_network})


@pytest.mark.verify
@pytest.mark.scenario(BRIDGED_VM)
def test_bridged_verify(scenario, host_network):
    _verify(BRIDGED_VM, scenario.record)
