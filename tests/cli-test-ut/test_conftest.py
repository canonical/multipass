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

"""Unit tests for fixtures and hooks in cli.conftest."""

import os
import re
import subprocess
import sys
import textwrap
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import MagicMock, call

import pytest

from cli import conftest
from cli.multipass import TestCaseFailure


class TestSessionFailureAbortsSession:
    """TestSessionFailure from any test phase must stop the session."""

    @staticmethod
    def _report(exc):
        def phase():
            if exc is not None:
                raise exc

        item = SimpleNamespace(session=SimpleNamespace(shouldfail=False))
        conftest.pytest_runtest_makereport(item, pytest.CallInfo.from_call(phase, "call"))
        return item.session.shouldfail

    @pytest.mark.parametrize(
        "exc", [None, TestCaseFailure("case"), RuntimeError("other")]
    )
    def test_other_outcomes_leave_session_running(self, exc):
        assert self._report(exc) is False

    def test_pytest_stops_after_session_failure(self, tmp_path):
        (tmp_path / "conftest.py").write_text(
            "from cli.conftest import pytest_runtest_makereport  # noqa: F401\n"
        )
        (tmp_path / "test_abort.py").write_text(
            textwrap.dedent(
                """\
                from cli.multipass import TestSessionFailure

                def test_first():
                    raise TestSessionFailure("daemon is gone")

                def test_second():
                    pass
                """
            )
        )
        tests_dir = str(Path(__file__).resolve().parent.parent)
        env = {**os.environ, "PYTHONPATH": os.pathsep.join(
            p for p in (tests_dir, os.environ.get("PYTHONPATH")) if p
        )}

        result = subprocess.run(
            [sys.executable, "-m", "pytest", "test_abort.py", "-p", "no:cacheprovider",
             "-o", "addopts=", "-rA"],
            cwd=tmp_path,
            capture_output=True,
            text=True,
            env=env,
            timeout=60,
            check=False,
        )

        output = result.stdout + result.stderr
        assert "test_abort.py::test_first" in output, output
        assert "test_second" not in output, output
        assert re.search(r"\b1 failed in\b", output), output
        assert " passed" not in output, output
        assert "daemon is gone" in output, output


class TestWindowsPrivilegedMounts:
    """Tests for the windows_privileged_mounts fixture."""

    def test_waits_for_restart_after_enabling(self, monkeypatch):
        monkeypatch.setattr(conftest.sys, "platform", "win32")
        mock_multipass = MagicMock(side_effect=["false\n", True])
        monkeypatch.setattr(conftest, "multipass", mock_multipass)
        mock_daemon = MagicMock()

        conftest.windows_privileged_mounts.__wrapped__(mock_daemon)

        assert mock_multipass.call_args_list == [
            call("get", "local.privileged-mounts"),
            call("set", "local.privileged-mounts=1"),
        ]
        mock_daemon.wait_for_restart.assert_called_once_with()

    def test_does_not_wait_when_already_enabled(self, monkeypatch):
        monkeypatch.setattr(conftest.sys, "platform", "win32")
        mock_multipass = MagicMock(return_value="true\n")
        monkeypatch.setattr(conftest, "multipass", mock_multipass)
        mock_daemon = MagicMock()

        conftest.windows_privileged_mounts.__wrapped__(mock_daemon)

        mock_multipass.assert_called_once_with("get", "local.privileged-mounts")
        mock_daemon.wait_for_restart.assert_not_called()

    def test_skips_when_multipassd_is_none_and_disabled(self, monkeypatch):
        monkeypatch.setattr(conftest.sys, "platform", "win32")
        mock_multipass = MagicMock(return_value="false\n")
        monkeypatch.setattr(conftest, "multipass", mock_multipass)

        with pytest.raises(pytest.skip.Exception):
            conftest.windows_privileged_mounts.__wrapped__(None)

        mock_multipass.assert_called_once_with("get", "local.privileged-mounts")

    def test_noop_on_non_windows(self, monkeypatch):
        monkeypatch.setattr(conftest.sys, "platform", "linux")
        mock_multipass = MagicMock()
        monkeypatch.setattr(conftest, "multipass", mock_multipass)
        mock_daemon = MagicMock()

        conftest.windows_privileged_mounts.__wrapped__(mock_daemon)

        mock_multipass.assert_not_called()
        mock_daemon.wait_for_restart.assert_not_called()


class TestRemoveTemporaryStorageDir:
    """Tests for remove_temporary_storage_dir()."""

    def test_removes_readonly_files(self, tmp_path, monkeypatch):
        import stat

        target = tmp_path / "storage"
        target.mkdir()
        ro_file = target / "cert.pem"
        ro_file.write_text("secret")
        os.chmod(ro_file, stat.S_IREAD)

        # POSIX can unlink read-only files; emulate Windows, which can't.
        real_unlink = os.unlink

        def windows_unlink(path, *args, dir_fd=None, **kwargs):
            mode = os.stat(path, dir_fd=dir_fd).st_mode
            if not mode & stat.S_IWRITE:
                raise PermissionError(f"read-only: {path}")
            real_unlink(path, *args, dir_fd=dir_fd, **kwargs)

        monkeypatch.setattr(os, "unlink", windows_unlink)

        conftest.remove_temporary_storage_dir(str(target))

        assert not target.exists()

    def test_retries_transient_permission_error(self, tmp_path, monkeypatch):
        import shutil
        import time

        target = tmp_path / "storage"
        target.mkdir()
        calls = {"count": 0}
        real_rmtree = shutil.rmtree

        def flaky_rmtree(path, **kwargs):
            calls["count"] += 1
            if calls["count"] < 3:
                raise PermissionError("file in use")
            real_rmtree(path, **kwargs)

        monkeypatch.setattr(shutil, "rmtree", flaky_rmtree)
        monkeypatch.setattr(time, "sleep", lambda _: None)

        conftest.remove_temporary_storage_dir(str(target), retries=5, delay=0)

        assert calls["count"] == 3
        assert not target.exists()

    def test_raises_after_exhausting_retries(self, tmp_path, monkeypatch):
        import shutil
        import time

        target = tmp_path / "storage"
        target.mkdir()
        monkeypatch.setattr(
            shutil, "rmtree", MagicMock(side_effect=PermissionError("locked"))
        )
        monkeypatch.setattr(time, "sleep", lambda _: None)

        with pytest.raises(PermissionError, match="locked"):
            conftest.remove_temporary_storage_dir(str(target), retries=3, delay=0)
