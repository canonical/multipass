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


from contextlib import contextmanager
import logging

from cli_tests.utilities import uuid4_str
from .multipass_cmd import multipass
from .helpers import mounts, state


@contextmanager
def launch(cfg_override=None):
    # Default configuration
    default_cfg = {
        "cpus": 2,
        "memory": "1G",
        "disk": "6G",
        "name": uuid4_str("instance"),
        "retry": 3,
        "image": "noble",
        "autopurge": True,
    }

    cfg = default_cfg

    if cfg_override:
        cfg.update(cfg_override)

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

    class VMHandle:
        def __init__(self, cfg: dict):
            self._cfg = cfg

        # give the helper something to stringify
        def __str__(self):
            return self.name

        # surface the config as attributes
        def __getattr__(self, item):
            try:
                return self._cfg[item]
            except KeyError as exc:
                raise AttributeError(item) from exc

        def __repr__(self):
            return f"<VMHandle {self.name}>"

    assert mounts(cfg["name"]) == {}
    assert state(cfg["name"]) == "Running"
    yield VMHandle(cfg)
    # yield cfg["name"]
    if cfg["autopurge"]:
        assert multipass("delete", cfg["name"], "--purge")
