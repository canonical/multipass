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

from cli_tests.utilities import merge, uuid4_str

from .helpers import mounts, multipass, state


@contextmanager
def launch(cfg_override=None):
    """Launch a VM with defaults (optionally overridden) and yield a handle, purging on exit by default."""

    # Default configuration
    default_cfg = {
        "cpus": 2,
        "memory": "1G",
        "disk": "6G",
        "retry": 3,
        "image": "noble",
        "autopurge": True,
        "assert": {"purge": True},
    }

    cfg = deepcopy(default_cfg)

    if cfg_override:
        merge(cfg, cfg_override)

    if "name" not in cfg:
        cfg["name"] = uuid4_str("instance")

    logging.debug(f"launch_new_instance: {cfg}")

    assert multipass(
        "launch",
        "--cpus",
        cfg["cpus"],
        "--memory",
        cfg["memory"],
        "--disk",
        cfg["disk"],
        "--name",
        cfg["name"],
        cfg["image"],
        retry=cfg["retry"],
    )

    class VMHandle(str):
        """String-like handle (the instance name) exposing config as attributes."""

        __slots__ = ("_cfg",)

        def __new__(cls, cfg: dict):
            obj = super().__new__(cls, cfg["name"])
            obj._cfg = cfg
            return obj

        # surface the config as attributes
        def __getattr__(self, item):
            try:
                return self._cfg[item]
            except KeyError as exc:
                raise AttributeError(item) from exc

    assert mounts(cfg["name"]) == {}
    assert state(cfg["name"]) == "Running"
    yield VMHandle(cfg)
    if cfg["autopurge"]:
        assert multipass("delete", cfg["name"], "--purge") or not cfg["assert"]["purge"]
