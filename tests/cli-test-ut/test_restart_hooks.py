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

"""Unit tests for the bounded post-restart boot id wait."""

import itertools

import pexpect
import pytest

from cli.multipass import multipass_cmd
from cli.multipass.cmd_output import Output

OLD_BOOT_ID = "11111111-1111-1111-1111-111111111111"
NEW_BOOT_ID = "22222222-2222-2222-2222-222222222222"

# Three polls fit in the restart timeout.
RESTART_TIMEOUT = 3 * (
    multipass_cmd.RESTART_POLL_DELAY + multipass_cmd.RESTART_POLL_EXEC_TIMEOUT
)


@pytest.fixture
def exec_calls(monkeypatch):
    """Record `exec` timeouts and answer from the outcomes set by the test."""
    monkeypatch.setattr(multipass_cmd.cfg.timeouts, "restart", RESTART_TIMEOUT, raising=False)
    monkeypatch.setattr("cli.utilities.functional.time.sleep", lambda _: None)
    calls = []

    def install(outcomes):
        outcomes = iter(outcomes)

        def fake_multipass(*args, **kwargs):
            calls.append(kwargs["timeout"])
            outcome = next(outcomes)
            if isinstance(outcome, BaseException):
                raise outcome
            return outcome

        monkeypatch.setattr(multipass_cmd, "multipass", fake_multipass)
        return calls

    return install


def test_returns_once_boot_id_changes(exec_calls):
    calls = exec_calls(
        [
            pexpect.exceptions.TIMEOUT("ssh not up yet"),
            Output(OLD_BOOT_ID, 0),
            Output(NEW_BOOT_ID, 0),
        ]
    )

    multipass_cmd._wait_for_new_boot_id("vm", OLD_BOOT_ID)

    assert calls == [multipass_cmd.RESTART_POLL_EXEC_TIMEOUT] * 3


def test_attempts_are_bounded_by_restart_timeout(exec_calls):
    calls = exec_calls(itertools.repeat(Output(OLD_BOOT_ID, 0)))

    with pytest.raises(AssertionError, match="did not come back with a new boot id after 3 attempts"):
        multipass_cmd._wait_for_new_boot_id("vm", OLD_BOOT_ID)

    assert len(calls) == 3
