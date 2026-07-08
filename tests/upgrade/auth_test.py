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

"""Authentication state should survive an upgrade: a set `local.passphrase` and
the already-authenticated primary client both persist across the package upgrade.

Passphrases are stored hashed, so `get local.passphrase` can't read one back; the
contract is tested by behaviour instead -- an unauthenticated (empty-HOME) client
is challenged, the wrong passphrase is rejected, the right one authenticates, and
the primary client (whose cert the governor authenticated at session setup) stays
authenticated."""

import sys

import pytest

from cli.config import cfg
from cli.multipass import multipass
from cli.utilities import TempDirectory

VM = "upg-auth"  # scenario key only; this concern launches no VM
PASSPHRASE = "upg-therewillbesecrets"


@pytest.mark.seed
@pytest.mark.scenario(VM)
def test_auth_seed(scenario, multipassd_session_scoped):
    assert multipass("set", f"local.passphrase={PASSPHRASE}")
    # Setting the passphrase restarts the daemon; wait for it to come back.
    multipassd_session_scoped.wait_for_restart()

    # The primary client (cert authenticated at session setup) still works.
    assert multipass("list")

    scenario.record.update({"passphrase_set": True})


@pytest.mark.verify
@pytest.mark.scenario(VM)
def test_auth_verify(scenario):

    # The already-authenticated primary client survived the upgrade.
    assert multipass("list")

    with TempDirectory() as empty_home:
        home = {"HOME": str(empty_home)}

        # A fresh (unauthenticated) client is still challenged -> passphrase persisted.
        # HOME override only bites on standalone/non-Windows: overriding HOME for snap
        # is convoluted and Qt on Windows ignores APPDATA overrides (see
        # cli/cli_authenticate_test.py).
        if cfg.daemon_controller == "standalone" and sys.platform != "win32":
            assert "Please authenticate" in multipass("list", env=home)

        # Wrong passphrase rejected, correct one still authenticates.
        assert "Passphrase is not correct" in multipass(
            "authenticate", PASSPHRASE + "x", env=home
        )
        assert multipass("authenticate", PASSPHRASE, env=home)
