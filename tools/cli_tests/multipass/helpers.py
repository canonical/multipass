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

"""Test helpers for running `multipass` commands and querying instance info."""

import sys
import re
import os
from pathlib import Path

from .multipass_cmd import multipass


def _retrieve_info_field(name, key):
    """Run `multipass info` for the given instance and return the specified field from the JSON output."""
    with multipass("info", "--format=json", f"{name}").json() as output:
        assert output
        return output["info"][name][key]


def debug_interactive_shell(name):
    """Open an interactive shell session inside the instance for debugging."""
    with multipass("shell", name, interactive=True) as shell:
        shell.interact()


def state(name):
    """Return the current state (e.g., 'Running', 'Stopped') of the instance."""
    return _retrieve_info_field(name, "state")


def mounts(name):
    """Return the mounts mapping for the instance."""
    return _retrieve_info_field(name, "mounts")


def snapshot_count(name):
    """Return the number of snapshots for the instance as an integer."""
    return int(_retrieve_info_field(name, "snapshot_count"))


def file_exists(vm_name, *paths):
    """Return True if all given paths exist in the instance, False otherwise."""
    return bool(
        multipass(
            "exec",
            vm_name,
            "--",
            "ls",
            *(Path(p).as_posix() for p in paths),
            timeout=180,
        )
    )


def read_file(vm_name, path):
    """Read the given file from the instance and return its contents as a string."""
    return str(
        multipass(
            "exec",
            vm_name,
            "--",
            "cat",
            Path(path).as_posix(),
            timeout=180,
        )
    )


def exec(name, *args, **kwargs):
    """Run the given command inside the instance."""
    return multipass("exec", name, "--", *args, **kwargs)


def shell(name):
    """
    Open an interactive shell in the instance.

    On Windows hosts, disable stdio buffering with `stdbuf` for better interactive
    behavior inside the Linux guest.
    """
    if sys.platform == "win32":
        # We have to disable buffering to get proper "interactive" shell
        # hence the `stdbuf` shenanigans.
        return multipass(
            "exec", name, "--", "stdbuf", "-oL", "-eL", "bash", "-i", interactive=True
        )
    else:
        # On Unix hosts, 'multipass shell' is fine.
        return multipass("shell", name, interactive=True)


def get_ram_size(name):
    """Return total RAM (in MiB) reported by /proc/meminfo inside the instance."""
    with multipass(
        "exec",
        name,
        "--",
        "cat /proc/meminfo",
    ) as result:
        assert result
        match = re.search(r"^MemTotal:\s+(\d+)\s+kB", str(result), re.MULTILINE)
        assert match
        mem_kb = int(match.group(1))
        return mem_kb // 1024


def get_disk_size(name):
    """Return root filesystem size (in MiB) inside the instance."""
    with multipass(
        "exec", name, "--", "bash -c 'df -m --output=size / | tail -1'"
    ) as result:
        assert result
        return int(result.content)


def get_core_count(name):
    """Return the number of CPU cores inside the instance."""
    with multipass("exec", name, "--", "nproc") as result:
        assert result
        return int(result.content)


def get_cloudinit_instance_id(name):
    assert file_exists(name, "/var/lib/cloud/data/instance-id")
    return read_file(name, "/var/lib/cloud/data/instance-id")


def get_default_interface_name(name):
    with multipass("exec", name, "--", "ip -o route get 1") as output:
        assert output
        # 1.0.0.0 via 192.168.35.1 dev br0 src 192.168.35.83 uid 1000 \    cache
        return output.content.split()[4].strip()


def get_mac_addr_of(name, interface_name):
    with multipass(
        "exec", name, "--", f"cat /sys/class/net/{interface_name}/address"
    ) as output:
        assert output
        return str(output).strip()


def default_driver_name():
    """Return the default Multipass driver for the current host platform."""

    #
    # https://docs.python.org/3/library/sys.html#sys.platform
    #
    # +-------------------+----------------+
    # | System            | platform value |
    # +-------------------+----------------+
    # | AIX               | 'aix'          |
    # | Android           | 'android'      |
    # | Emscripten        | 'emscripten'   |
    # | iOS               | 'ios'          |
    # | Linux             | 'linux'        |
    # | macOS             | 'darwin'       |
    # | Windows           | 'win32'        |
    # | Windows/Cygwin    | 'cygwin'       |
    # | WASI              | 'wasi'         |
    # +-------------------+----------------+

    platform_driver_mappings = {"win32": "hyperv", "linux": "qemu", "darwin": "qemu"}

    if sys.platform in platform_driver_mappings:
        return platform_driver_mappings[sys.platform]

    raise NotImplementedError(f"Unsupported platform: {sys.platform}")


def default_mount_gid():
    """Return default GID for mounted files inside the instance."""
    if sys.platform == "win32":
        return -2
    # Use gid of the user that's running the tests
    return os.getgid()


def default_mount_uid():
    """Return default UID for mounted files inside the instance."""
    if sys.platform == "win32":
        return -2
    # Use uid of the user that's running the tests
    return os.getuid()
