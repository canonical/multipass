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

"""Unit tests for the retry decorator.

Both `time.sleep` and `asyncio.sleep` are mocked for every test, so no test
actually waits or depends on timing.
"""

import asyncio
from types import SimpleNamespace
from unittest import mock

import pexpect
import pytest

from cli.utilities import functional
from cli.utilities.functional import retry


@pytest.fixture(autouse=True)
def sleeps():
    with mock.patch.object(functional.time, "sleep") as sync_sleep, mock.patch.object(
        functional.asyncio, "sleep", new_callable=mock.AsyncMock
    ) as async_sleep:
        yield SimpleNamespace(sync=sync_sleep, async_=async_sleep)


class TestRetrySync:
    def test_returns_first_successful_result(self, sleeps):
        fn = mock.Mock(side_effect=[False, None, "ok"])
        assert retry(retries=5, delay=0.5)(fn)() == "ok"
        assert fn.call_count == 3
        assert sleeps.sync.call_args_list == [mock.call(0.5)] * 2

    def test_returns_last_result_when_retries_exhausted(self, sleeps):
        fn = mock.Mock(side_effect=[0, 0, 0])
        assert retry(retries=2, delay=0.5)(fn)() == 0
        assert fn.call_count == 3
        # No sleep after the final attempt.
        assert sleeps.sync.call_args_list == [mock.call(0.5)] * 2

    def test_success_on_first_attempt_does_not_sleep(self, sleeps):
        fn = mock.Mock(return_value=True)
        assert retry(retries=3, delay=0.5)(fn)() is True
        fn.assert_called_once()
        sleeps.sync.assert_not_called()

    @pytest.mark.parametrize("bad_status", [1, None], ids=["nonzero", "killed"])
    def test_unsuccessful_exitstatus_is_retried(self, bad_status):
        bad = SimpleNamespace(exitstatus=bad_status)
        good = SimpleNamespace(exitstatus=0)
        fn = mock.Mock(side_effect=[bad, good])
        assert retry(retries=3, delay=0.5)(fn)() is good
        assert fn.call_count == 2

    def test_pexpect_timeout_is_retried(self):
        fn = mock.Mock(side_effect=[pexpect.exceptions.TIMEOUT("t/o"), True])
        assert retry(retries=1, delay=0.5)(fn)() is True
        assert fn.call_count == 2

    def test_other_exceptions_propagate_without_retry(self, sleeps):
        fn = mock.Mock(side_effect=[ValueError("boom"), True])
        with pytest.raises(ValueError, match="boom"):
            retry(retries=3, delay=0.5)(fn)()
        fn.assert_called_once()
        sleeps.sync.assert_not_called()

    def test_custom_success_fn(self):
        fn = mock.Mock(side_effect=[(1, "busy"), (1, "busy"), (1, "fatal")])
        result = retry(retries=5, delay=0.5, success_fn=lambda r: r[1] != "busy")(fn)()
        assert result == (1, "fatal")
        assert fn.call_count == 3

    def test_passes_arguments(self):
        fn = mock.Mock(return_value=True)
        retry(retries=0, delay=0.5)(fn)(1, b=2)
        fn.assert_called_once_with(1, b=2)


class TestRetryAsync:
    def test_retries_until_success(self, sleeps):
        fn = mock.AsyncMock(side_effect=[False, None, "ok"])
        assert asyncio.run(retry(retries=5, delay=0.5)(fn)()) == "ok"
        assert fn.await_count == 3
        assert sleeps.async_.await_args_list == [mock.call(0.5)] * 2

    def test_returns_last_result_when_retries_exhausted(self, sleeps):
        fn = mock.AsyncMock(side_effect=[False, False])
        assert asyncio.run(retry(retries=1, delay=0.5)(fn)()) is False
        assert fn.await_count == 2
        # No sleep after the final attempt.
        assert sleeps.async_.await_args_list == [mock.call(0.5)]

    def test_does_not_block_event_loop(self, sleeps):
        fn = mock.AsyncMock(side_effect=[False, False, True])
        assert asyncio.run(retry(retries=2, delay=0.5)(fn)()) is True
        sleeps.sync.assert_not_called()
        assert sleeps.async_.await_count == 2

    def test_custom_success_fn_and_arguments(self):
        fn = mock.AsyncMock(side_effect=[1, 2, 3])
        wrapped = retry(retries=3, delay=0.5, success_fn=lambda r: r >= 2)(fn)
        assert asyncio.run(wrapped("arg", key="value")) == 2
        assert fn.await_args_list == [mock.call("arg", key="value")] * 2

    def test_pexpect_timeout_is_retried(self):
        fn = mock.AsyncMock(side_effect=[pexpect.exceptions.TIMEOUT("t/o"), True])
        assert asyncio.run(retry(retries=1, delay=0.5)(fn)()) is True
        assert fn.await_count == 2

    def test_other_exceptions_propagate_without_retry(self, sleeps):
        fn = mock.AsyncMock(side_effect=[RuntimeError("fatal"), True])
        with pytest.raises(RuntimeError, match="fatal"):
            asyncio.run(retry(retries=3, delay=0.5)(fn)())
        fn.assert_awaited_once()
        sleeps.async_.assert_not_awaited()

    def test_cancellation_during_wait_stops_retrying(self, sleeps):
        sleeps.async_.side_effect = asyncio.CancelledError
        fn = mock.AsyncMock(return_value=False)
        with pytest.raises(asyncio.CancelledError):
            asyncio.run(retry(retries=100, delay=0.5)(fn)())
        fn.assert_awaited_once()
