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

"""Ephemeral host networking for the network upgrade test (Linux only).

A bridge created with ``ip link add ... type bridge`` shows up in
``multipass networks`` (Multipass treats any ``/sys/class/net/<x>/bridge`` as a
usable bridge) and can be attached to a VM with ``--network``. Crucially it is
*runtime-only*: nothing is written to netplan/systemd, so it is gone after a
reboot. The test still deletes it explicitly; the reboot-volatility is just the
backstop if a run is interrupted.
"""

import logging
import subprocess

from cli.utilities import sudo


def create_ephemeral_bridge(name: str) -> None:
    """Create (idempotently) and bring up a runtime-only host bridge."""
    delete_ephemeral_bridge(name)  # clear any leftover from an interrupted run
    logging.info("upgrade-net :: creating ephemeral bridge `%s`", name)
    subprocess.run([*sudo("ip", "link", "add", "name", name, "type", "bridge")], check=True)
    subprocess.run([*sudo("ip", "link", "set", name, "up")], check=True)


def delete_ephemeral_bridge(name: str) -> None:
    """Delete the bridge if present (safe to call when it does not exist)."""
    subprocess.run(
        [*sudo("ip", "link", "delete", name)],
        check=False,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
