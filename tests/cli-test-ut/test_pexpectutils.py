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

"""Unit tests for WinptySpawn in cli.utilities.pexpectutils."""

import importlib
import socket
import sys
import types
from queue import Queue
from unittest.mock import MagicMock

import pytest


@pytest.fixture
def winpty_spawn_cls(monkeypatch):
    """Import WinptySpawn portably, stubbing winpty when not on Windows."""
    module_name = "cli.utilities.pexpectutils"
    if sys.platform == "win32":
        module = importlib.import_module(module_name)
        yield module.WinptySpawn
        return

    fake_winpty = types.ModuleType("winpty")
    fake_winpty.Backend = MagicMock()
    fake_winpty.PtyProcess = MagicMock()
    monkeypatch.setattr(sys, "platform", "win32")
    monkeypatch.setitem(sys.modules, "winpty", fake_winpty)
    monkeypatch.delitem(sys.modules, module_name, raising=False)
    module = importlib.import_module(module_name)
    try:
        yield module.WinptySpawn
    finally:
        sys.modules.pop(module_name, None)


def _make_spawn(winpty_spawn_cls, pty_proc):
    spawn = object.__new__(winpty_spawn_cls)
    spawn.pty_proc = pty_proc
    spawn._read_queue = Queue()
    return spawn


def _drain_queue(q):
    items = []
    while not q.empty():
        items.append(q.get_nowait())
    return items


class TestWinptySpawnReadIncoming:
    """Tests for WinptySpawn._read_incoming()."""

    @pytest.mark.parametrize(
        "exc",
        [
            EOFError(),
            OSError(10038, "An operation was attempted on something that is not a socket"),
        ],
        ids=["eof-error", "os-error"],
    )
    def test_expected_errors_stop_loop_and_enqueue_eof(self, winpty_spawn_cls, exc):
        pty_proc = MagicMock()
        pty_proc.read.side_effect = ["hello\r\n", exc]
        spawn = _make_spawn(winpty_spawn_cls, pty_proc)

        spawn._read_incoming()

        pty_proc.fileobj.settimeout.assert_called_once_with(1.0)
        assert _drain_queue(spawn._read_queue) == ["hello\r\n", None]

    def test_unexpected_error_still_enqueues_eof(self, winpty_spawn_cls):
        pty_proc = MagicMock()
        pty_proc.read.side_effect = ["partial", RuntimeError("boom")]
        spawn = _make_spawn(winpty_spawn_cls, pty_proc)

        with pytest.raises(RuntimeError, match="boom"):
            spawn._read_incoming()

        assert _drain_queue(spawn._read_queue) == ["partial", None]

    def test_socket_timeout_exits_when_process_dies(self, winpty_spawn_cls):
        pty_proc = MagicMock()
        pty_proc.read.side_effect = [socket.timeout(), "output", socket.timeout()]
        pty_proc.isalive.side_effect = [True, False]
        spawn = _make_spawn(winpty_spawn_cls, pty_proc)

        spawn._read_incoming()

        assert pty_proc.isalive.call_count == 2
        assert _drain_queue(spawn._read_queue) == ["output", None]
