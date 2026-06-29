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

"""Assertions and small building blocks shared by the upgrade tests."""

import sys

from cli.multipass import (
    info,
    multipass,
    read_file,
    state,
    write_file,
    exec as guest_exec,
    get_mac_addr_of,
)
from cli.utilities import uuid4_str

#: Sentinel files live in the guest home, which survives reboots and snapshots.
SENTINEL_DIR = "/home/ubuntu"


def enable_privileged_mounts(governor):
    """Windows-only, idempotent: the mount tests need privileged mounts on.
    Enabling it restarts the daemon, so wait for it to come back. Call once at
    session setup, before any VM is launched -- then the restart auto-starts
    nothing, so there is no start race to handle."""
    if sys.platform != "win32":
        return
    if "true" in multipass("get", "local.privileged-mounts"):
        return
    assert multipass("set", "local.privileged-mounts=1")
    governor.wait_for_restart()


def make_sentinel(label: str) -> str:
    """Unique content to embed in a guest and later compare byte for byte."""
    return f"upgrade-sentinel::{label}::{uuid4_str()}"


def sentinel_path(label: str) -> str:
    return f"{SENTINEL_DIR}/upgrade-sentinel-{label}.txt"


def write_sentinel(vm_name: str, label: str, content: str) -> str:
    """Write ``content`` to a labelled sentinel file in the guest; return its path."""
    path = sentinel_path(label)
    assert write_file(vm_name, path, content), f"Failed to write sentinel `{path}`"
    return path


def assert_sentinel(vm_name: str, path: str, expected: str) -> None:
    """Assert the guest file at ``path`` still holds exactly ``expected``."""
    actual = read_file(vm_name, path).strip()
    assert actual == expected, (
        f"Sentinel mismatch in `{vm_name}:{path}`: expected `{expected}`, found `{actual}`"
    )


def seed_sentinel(vm_name: str, label: str) -> dict:
    """Write a fresh labelled sentinel into the guest and return its record.

    Bundles the sentinel's path and content into one dict, so a seed test can
    stash it under a single manifest key and the verify phase can replay it with
    ``assert_sentinel_record``.
    """
    content = make_sentinel(label)
    path = write_sentinel(vm_name, label, content)
    return {"path": path, "content": content}


def assert_sentinel_record(vm_name: str, record: dict) -> None:
    """Assert the guest still holds the sentinel captured by ``seed_sentinel``."""
    assert_sentinel(vm_name, record["path"], record["content"])


def park_seeded(vm_name: str, how: str = "stop", expected: str = "Stopped") -> None:
    """Drive a seeded VM into its parked state and confirm it landed there.

    The inverse of ``resume_seeded``: seed tests end by parking the VM in a
    precise ``Stopped`` (``how="stop"``) or ``Suspended`` (``how="suspend"``)
    state, so the verify phase has a version-independent expectation.
    """
    assert multipass(how, vm_name), f"`multipass {how} {vm_name}` failed"
    assert state(vm_name) == expected, (
        f"`{vm_name}` did not reach `{expected}` after `{how}`"
    )


def resume_seeded(vm_name: str, expected_state: str = "Stopped") -> None:
    """Assert the VM came back in its expected post-seed state, then boot it.

    A refresh must not have silently started, deleted, or otherwise disturbed a
    seeded VM before we bring it back up.
    """
    actual = state(vm_name)
    assert actual == expected_state, (
        f"`{vm_name}` came back as `{actual}`, expected `{expected_state}`"
    )
    assert multipass("start", vm_name)
    assert state(vm_name) == "Running"


def info_fingerprint(vm_name: str) -> dict:
    """Host-reported instance facts that must be stable across an upgrade.

    ``cpu_count`` and ``memory.total`` are only populated while the instance is
    *running*, so both capture (seed) and comparison (verify) must happen with
    the VM up. ``image_release`` is stable in any state.
    """
    fields = info(vm_name)[vm_name]
    return {
        "cpu_count": fields.get("cpu_count"),
        "image_release": fields.get("image_release"),
        "memory_total": fields.get("memory", {}).get("total"),
    }


def assert_fingerprint_unchanged(vm_name: str, recorded: dict) -> None:
    current = info_fingerprint(vm_name)
    assert current == recorded, (
        f"Instance fingerprint for `{vm_name}` changed across upgrade: "
        f"recorded {recorded}, now {current}"
    )


def guest_interface_macs(vm_name: str) -> list:
    """Sorted MAC addresses of the guest's network interfaces (excluding loopback).

    MACs of the default NIC and any extra (``--network``) interfaces are
    persisted in the instance record, so they are a stable, host-independent
    signal that an interface survived an upgrade.
    """
    listing = guest_exec(vm_name, "ls", "/sys/class/net")
    assert listing, f"Could not enumerate interfaces on `{vm_name}`: {listing}"
    interfaces = [iface for iface in listing.content.split() if iface != "lo"]
    return sorted(get_mac_addr_of(vm_name, iface) for iface in interfaces)
