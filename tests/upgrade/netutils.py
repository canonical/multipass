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

"""Host networking for the network upgrade test, across platforms.

The test attaches an extra ``--network`` interface to a seed VM and checks the
NIC (its persisted MAC) survives the upgrade. Multipass validates ``--network``
against the host networks it can enumerate, and only some of those can be
attached *non-interactively*: bridging to a network that ``needs_authorization``
triggers a yes/no prompt that a non-interactive client always declines. So each
phase stands up its own isolated, runtime-only host network -- one that is both
enumerated by ``multipass networks`` and needs no authorization -- attaches to
it, and tears it down afterwards. Privileged creation is escalated the same way
the cli tests do it (``sudo`` on Unix, ``gsudo`` on Windows).

* **Linux** -- a throwaway host *bridge* (``ip link add ... type bridge``).
  Bridges need no authorization and are runtime-only (gone after a reboot).
* **Windows** -- a *private* Hyper-V vSwitch (``New-VMSwitch -SwitchType
  Private``). ``Get-VMSwitch`` lists it (so ``--network`` accepts it), it needs
  no authorization, and a private switch has no host/external uplink, so it
  never disrupts host connectivity. ``New-VMSwitch`` ships with the Hyper-V
  PowerShell module, which is absent on Windows *Home* (and on the VirtualBox
  backend Hyper-V is not in play at all); where it -- or a Hyper-V backend -- is
  unavailable, the test is skipped, as there is then no isolated network to
  attach. (HCN, the API Multipass's experimental ``hyperv_api`` backend uses, is
  more broadly available but has no built-in cmdlet, so it is not a practical
  substitute here.)
* **macOS** -- *skipped*. No ephemeral option exists: both backends only bridge
  onto physical NICs (qemu via ``vmnet-bridged``, applevz via
  ``VZBridgedNetworkInterface``), so a host-created bridge is never enumerable
  by ``multipass networks`` and cannot be attached. There is no isolated network
  to stand up, so the test does not run there.

The bridge/switch is re-created with the same fixed name in each phase, so the
instance's persisted NIC re-attaches by name on the verify run.
"""

import logging
import subprocess
import sys
from contextlib import contextmanager

import pytest

from cli.config import cfg
from cli.utilities import sudo

#: Name of the throwaway Linux bridge (runtime-only, isolated, no authorization).
LINUX_BRIDGE = "mpupgtbr0"

#: Name of the throwaway Windows private Hyper-V vSwitch (isolated, no uplink).
WINDOWS_SWITCH = "mpupgtsw0"

#: Multipass backends whose ``--network`` accepts a Hyper-V vSwitch (so the
#: ``New-VMSwitch`` path applies). Other Windows backends (e.g. ``virtualbox``,
#: used on Windows Home) enumerate only physical NICs, so there is nothing
#: isolated to attach and the test is skipped.
_HYPERV_DRIVERS = ("hyperv", "hyperv_api")


def _create_linux_bridge(name: str) -> None:
    _delete_linux_bridge(name)  # clear any leftover from an interrupted run
    logging.info("upgrade-net :: creating ephemeral bridge `%s`", name)
    subprocess.run([*sudo("ip", "link", "add", "name", name, "type", "bridge")], check=True)
    subprocess.run([*sudo("ip", "link", "set", name, "up")], check=True)


def _delete_linux_bridge(name: str) -> None:
    subprocess.run(
        [*sudo("ip", "link", "delete", name)],
        check=False,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )


def _powershell(script: str, *, check: bool):
    """Run an elevated PowerShell one-liner (``gsudo`` via the cli helpers)."""
    return subprocess.run(
        [*sudo("powershell", "-NoProfile", "-NonInteractive", "-Command", script)],
        check=check,
        stdout=subprocess.DEVNULL if not check else None,
        stderr=subprocess.DEVNULL if not check else None,
    )


def _new_vmswitch_available() -> bool:
    """Whether the ``New-VMSwitch`` cmdlet exists (absent on Windows Home).

    Probed without elevation -- ``Get-Command`` needs no privileges, and this
    avoids a UAC prompt just to discover the host can't run the test.
    """
    probe = subprocess.run(
        [
            "powershell", "-NoProfile", "-NonInteractive", "-Command",
            "if (Get-Command New-VMSwitch -ErrorAction SilentlyContinue) "
            "{ exit 0 } else { exit 1 }",
        ],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    return probe.returncode == 0


def _create_windows_switch(name: str) -> None:
    _delete_windows_switch(name)  # clear any leftover from an interrupted run
    logging.info("upgrade-net :: creating ephemeral Hyper-V switch `%s`", name)
    _powershell(f"New-VMSwitch -Name '{name}' -SwitchType Private | Out-Null", check=True)


def _delete_windows_switch(name: str) -> None:
    _powershell(
        f"Remove-VMSwitch -Name '{name}' -Force -ErrorAction SilentlyContinue", check=False
    )


@contextmanager
def upgrade_network():
    """Yield the name of a freshly-created, isolated host network to attach to.

    Linux/Windows create it (and tear it down) per phase, re-using the same
    fixed name so the instance's persisted NIC re-attaches by name on verify.
    The test is skipped where no isolated ephemeral network can be created (see
    module docstring): macOS always, and Windows when a non-Hyper-V backend is
    in use or ``New-VMSwitch`` is unavailable (e.g. Windows Home).
    """
    if sys.platform == "linux":
        _create_linux_bridge(LINUX_BRIDGE)
        try:
            yield LINUX_BRIDGE
        finally:
            _delete_linux_bridge(LINUX_BRIDGE)
    elif sys.platform == "win32":
        if cfg.driver not in _HYPERV_DRIVERS:
            pytest.skip(
                f"`{cfg.driver}` backend enumerates only physical NICs, so no "
                "isolated ephemeral network can be attached on Windows"
            )
        if not _new_vmswitch_available():
            pytest.skip(
                "`New-VMSwitch` is unavailable (e.g. Windows Home, which lacks the "
                "Hyper-V PowerShell module); cannot create an ephemeral vSwitch"
            )
        _create_windows_switch(WINDOWS_SWITCH)
        try:
            yield WINDOWS_SWITCH
        finally:
            _delete_windows_switch(WINDOWS_SWITCH)
    else:
        pytest.skip(
            "network upgrade test needs an isolated ephemeral host network, which "
            "cannot be created on macOS (both backends only bridge onto physical NICs)"
        )
