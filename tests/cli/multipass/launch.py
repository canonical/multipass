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

"""Context manager for launching a Multipass VM in CLI tests."""

import logging
from contextlib import contextmanager
from copy import deepcopy

from cli.utilities import merge
from cli.config import cfg

from .nameutils import random_vm_name
from .helpers import mounts, multipass, state, vm_exists


@contextmanager
def launch(cfg_override=None):
    """Launch a VM with defaults (optionally overridden) and yield a
    handle, purging on exit by default."""

    # Default configuration
    default_vm_cfg = {
        "cpus": cfg.vm.cpus,
        "memory": cfg.vm.memory,
        "disk": cfg.vm.disk,
        "retry": getattr(cfg.retries, "launch", 0),
        "image": cfg.vm.image,
        "autopurge": True,
        "assert": {"purge": True},
    }

    vm_cfg = deepcopy(default_vm_cfg)

    if cfg_override:
        merge(vm_cfg, cfg_override)

    if "name" not in vm_cfg:
        vm_cfg["name"] = random_vm_name()

    logging.debug(f"launch_new_instance: {vm_cfg}")

    assert not vm_exists(vm_cfg["name"])

    with multipass(
        "launch",
        "--cpus",
        vm_cfg["cpus"],
        "--memory",
        vm_cfg["memory"],
        "--disk",
        vm_cfg["disk"],
        "--name",
        vm_cfg["name"],
        "--timeout",
        getattr(cfg.timeouts, "launch", 300),
        vm_cfg["image"],
        retry=vm_cfg["retry"],
    ) as launch_r:
        # The launch does not have a dedicated exit code for the "already exists".
        already_exists_str = f"instance \"{vm_cfg['name']}\" already exists"
        already_exists = already_exists_str in str(launch_r)
        assert (
            already_exists or launch_r), f"Failed to launch VM `{vm_cfg['name']}`: {str(launch_r)}"

    class VMHandle(str):
        """String-like handle (the instance name) exposing config as attributes."""

        __slots__ = ("_vm_cfg",)

        def __new__(cls, vm_cfg: dict):
            obj = super().__new__(cls, vm_cfg["name"])
            obj._vm_cfg = vm_cfg
            return obj

        # surface the config as attributes
        def __getattr__(self, item):
            try:
                return self._vm_cfg[item]
            except KeyError as exc:
                raise AttributeError(item) from exc

    assert mounts(vm_cfg["name"]) == {}
    assert state(vm_cfg["name"]) == "Running"

    yield VMHandle(vm_cfg)

    if vm_cfg["autopurge"]:
        with multipass("delete", vm_cfg["name"], "--purge") as result:
            if vm_cfg["assert"]["purge"]:
                assert result, f"Failed to purge VM `{vm_cfg['name']}`: {str(result)}"
