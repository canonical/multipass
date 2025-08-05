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

import sys
from pathlib import Path

from .multipass_cmd import multipass


def _retrieve_info_field(name, key):
    with multipass("info", "--format=json", f"{name}").json() as output:
        assert output
        return output["info"][name][key]


def debug_interactive_shell(name):
    with multipass("shell", name, interactive=True) as shell:
        shell.interact()


def state(name):
    """Retrieve state of a VM"""
    return _retrieve_info_field(name, "state")


def mounts(name):
    return _retrieve_info_field(name, "mounts")


def file_exists(vm_name, *paths):
    """Return True if a file exists in the given VM, False otherwise."""
    return multipass(
        "exec",
        f"{vm_name}",
        "--",
        "ls",
        *(Path(p).as_posix() for p in paths),
        timeout=180,
    )


def exec(name, *args, **kwargs):
    return multipass("exec", name, "--", *args, **kwargs)


def shell(name):
    # We have to disable buffering to get proper "interactive" shell
    # hence the `stdbuf` shenanigans.
    if sys.platform == "win32":
        return multipass(
            "exec", name, "--", "stdbuf", "-oL", "-eL", "bash", "-i", interactive=True
        )
    else:
        return multipass("shell", name, interactive=True)


def get_ram_size(name):
    with multipass(
        "exec",
        name,
        "--",
        "awk '/MemTotal/ { printf \"%d\\n\", $2 / 1024 }' /proc/meminfo",
    ) as result:
        assert result
        return result.content


def get_disk_size(name):
    with multipass(
        "exec", name, "--", "bash -c 'df -m --output=size / | tail -1'"
    ) as result:
        assert result
        return result.content


def get_core_count(name):
    with multipass("exec", name, "--", "nproc") as result:
        assert result
        return int(result.content)


def default_driver_name():

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

    return NotImplementedError


def default_mount_gid():
    if sys.platform == "win32":
        return -2
    return 1000


def default_mount_uid():
    if sys.platform == "win32":
        return -2
    return 1000
