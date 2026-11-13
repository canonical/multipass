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

"""Unit tests for waiting on the Multipass CLI before a test session."""

from contextlib import contextmanager
from unittest.mock import patch

import pexpect
import pytest

from cli.multipass import helpers
from cli.multipass.cmd_output import Output

FAILED = Output("arbitrary failure output", 1)
OK = Output("", 0)
HANG = object()


@contextmanager
def fake_cli(outputs, ticks=None):
    """Replace the CLI with canned results and record (args, kwargs) per call."""
    calls = []

    @contextmanager
    def run(*args, **kwargs):
        calls.append((args, kwargs))
        output = outputs.pop(0)
        if output is HANG:
            raise pexpect.TIMEOUT("hung")
        yield output

    with (
        patch.object(helpers, "multipass", run),
        patch.object(helpers.time, "sleep"),
    ):
        if ticks is None:
            yield calls
        else:
            with patch.object(helpers.time, "monotonic", lambda: next(ticks)):
                yield calls


def test_retries_version_until_it_succeeds():
    with fake_cli([FAILED, HANG, OK]) as calls:
        helpers.wait_for_cli_ready(60)

    assert [args for args, _ in calls] == [("version",)] * 3
    assert all(kwargs["retry"] == 0 for _, kwargs in calls)


def test_bounds_each_attempt_by_remaining_time():
    with fake_cli([FAILED, OK], ticks=iter([0, 10, 50])) as calls:
        helpers.wait_for_cli_ready(60)

    assert [kwargs["timeout"] for _, kwargs in calls] == [50, 10]


@pytest.mark.parametrize(
    "outputs,message",
    [
        ([FAILED], "arbitrary failure output"),
        ([HANG], "did not finish within 59s"),
    ],
)
def test_raises_after_timeout(outputs, message):
    with (
        fake_cli(outputs, ticks=iter([0, 1, 100])),
        pytest.raises(TimeoutError, match=message),
    ):
        helpers.wait_for_cli_ready(60)
