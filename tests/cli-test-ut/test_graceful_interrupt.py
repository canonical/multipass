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

"""Unit and subprocess tests for graceful Ctrl-C / KeyboardInterrupt handling."""

import os
import signal
import subprocess
import sys
import textwrap
import time
from pathlib import Path
from unittest.mock import AsyncMock, MagicMock, patch

import pytest

from cli import conftest
from cli.config import cfg
from cli.controller.multipassd_governor import MultipassdGovernor
from cli.utilities.functional import retry
from cli.utilities.procutils import send_ctrl_c
from governor_helpers import MockController


def _wait_for_file(path: Path, timeout: float = 15.0, proc=None) -> None:
    deadline = time.monotonic() + timeout
    while not path.exists():
        if proc is not None and proc.poll() is not None:
            raise RuntimeError(
                f"Subprocess exited early with code {proc.returncode} before creating {path}"
            )
        if time.monotonic() > deadline:
            raise TimeoutError(f"Timed out waiting for {path}")
        time.sleep(0.05)


class TestKeyboardInterruptUnit:
    """In-process unit tests for KeyboardInterrupt teardown."""

    def test_multipassd_impl_stops_daemon_on_keyboard_interrupt(self, monkeypatch):
        ctrl = MockController(exit_code=None)
        monkeypatch.setattr(cfg, "daemon_controller", "standalone", raising=False)
        monkeypatch.setattr(cfg, "remove_all_instances", False, raising=False)
        monkeypatch.setattr(cfg, "print_daemon_output", False, raising=False)
        monkeypatch.setattr(cfg, "active_governor", None, raising=False)
        monkeypatch.setattr(conftest, "make_daemon_controller", lambda _: lambda: ctrl)
        monkeypatch.setattr(conftest, "set_driver", MagicMock())

        with (
            patch.object(
                MultipassdGovernor,
                "_ensure_client_certs_are_created",
                new_callable=AsyncMock,
            ),
            patch.object(MultipassdGovernor, "_authenticate_client_cert"),
            patch.object(
                MultipassdGovernor,
                "wait_for_multipassd_ready",
                new_callable=AsyncMock,
                return_value=True,
            ),
            patch.object(MultipassdGovernor, "on_monitor_exit", new_callable=AsyncMock),
        ):
            with pytest.raises(KeyboardInterrupt):
                with conftest.multipassd_impl() as governor:
                    assert governor is not None
                    assert governor.daemon_ready_event.is_set()
                    assert cfg.active_governor is governor
                    ctrl.stop_called = False
                    raise KeyboardInterrupt

        assert ctrl.stop_called
        assert cfg.active_governor is None

    def test_retry_propagates_keyboard_interrupt_without_retrying(self):
        calls = 0

        @retry(retries=3, delay=0)
        def interrupted():
            nonlocal calls
            calls += 1
            raise KeyboardInterrupt

        with pytest.raises(KeyboardInterrupt):
            interrupted()

        assert calls == 1

    @pytest.mark.skipif(sys.platform != "win32", reason="Windows-only")
    def test_send_ctrl_c_gracefully_terminates_child_on_windows(self, tmp_path):
        ready_file = tmp_path / "ready"
        caught_file = tmp_path / "caught"
        child_script = tmp_path / "child.py"
        child_script.write_text(
            textwrap.dedent(
                f"""\
                import signal, sys, time
                from pathlib import Path

                def on_sigint(signum, frame):
                    Path({str(caught_file)!r}).write_text("caught")
                    sys.exit(0)

                signal.signal(signal.SIGINT, on_sigint)
                Path({str(ready_file)!r}).write_text("ready")
                while True:
                    time.sleep(0.05)
                """
            )
        )

        proc = subprocess.Popen(
            [sys.executable, str(child_script)],
            creationflags=(
                subprocess.CREATE_NEW_PROCESS_GROUP | subprocess.CREATE_NEW_CONSOLE
            ),
        )
        try:
            _wait_for_file(ready_file)
            send_ctrl_c(proc.pid)
            assert proc.wait(timeout=10) == 0
            assert caught_file.read_text() == "caught"
        finally:
            if proc.poll() is None:
                proc.kill()
                proc.wait(timeout=5)


class TestPytestSubprocessCtrlC:
    """End-to-end subprocess test sending a real OS interrupt mid-pytest."""

    def test_pytest_subprocess_graceful_ctrl_c_teardown(self, tmp_path):
        daemon_ready = tmp_path / "daemon_ready"
        daemon_stopped = tmp_path / "daemon_stopped"
        test_ready = tmp_path / "test_ready"
        storage_record = tmp_path / "storage_dir.txt"

        fake_daemon = tmp_path / "fake_daemon.py"
        fake_daemon.write_text(
            textwrap.dedent(
                f"""\
                import signal, sys, time
                from pathlib import Path

                def on_stop(signum, frame):
                    Path({str(daemon_stopped)!r}).write_text("stopped")
                    sys.exit(0)

                signal.signal(signal.SIGINT, on_stop)
                signal.signal(signal.SIGTERM, on_stop)
                Path({str(daemon_ready)!r}).write_text("ready")
                while True:
                    time.sleep(0.05)
                """
            )
        )

        test_file = tmp_path / "test_ctrl_c_target.py"
        test_file.write_text(
            textwrap.dedent(
                f"""\
                import os, stat, subprocess, sys, time
                from pathlib import Path
                from unittest.mock import AsyncMock, MagicMock, patch

                from cli import conftest
                from cli.config import cfg
                from cli.controller.multipassd_governor import MultipassdGovernor
                from cli.controller.standalone_multipassd_controller import (
                    StandaloneMultipassdController,
                )


                class FakeStandaloneController(StandaloneMultipassdController):
                    def __init__(self):
                        self._argv = [sys.executable, {str(fake_daemon)!r}]
                        self._kwargs = {{}}
                        if sys.platform == "win32":
                            self._kwargs["creationflags"] = (
                                subprocess.CREATE_NEW_PROCESS_GROUP
                                | subprocess.CREATE_NEW_CONSOLE
                            )
                        self._proc = None
                        self._env = os.environ.copy()


                def test_interrupted_mid_run(monkeypatch):
                    monkeypatch.setattr(cfg, "daemon_controller", "standalone", raising=False)
                    monkeypatch.setattr(cfg, "remove_all_instances", False, raising=False)
                    monkeypatch.setattr(cfg, "print_daemon_output", False, raising=False)
                    monkeypatch.setattr(cfg, "active_governor", None, raising=False)

                    storage_dir = conftest.make_temporary_storage_dir()
                    ro_file = Path(storage_dir) / "cert.pem"
                    ro_file.write_text("dummy")
                    os.chmod(ro_file, stat.S_IREAD)
                    Path({str(storage_record)!r}).write_text(storage_dir)

                    ctrl = FakeStandaloneController()
                    monkeypatch.setattr(
                        conftest, "make_daemon_controller", lambda _: lambda: ctrl
                    )
                    monkeypatch.setattr(conftest, "set_driver", MagicMock())

                    with (
                        patch.object(
                            MultipassdGovernor,
                            "_ensure_client_certs_are_created",
                            new_callable=AsyncMock,
                        ),
                        patch.object(MultipassdGovernor, "_authenticate_client_cert"),
                        patch.object(
                            MultipassdGovernor,
                            "wait_for_multipassd_ready",
                            new_callable=AsyncMock,
                            return_value=True,
                        ),
                        patch.object(
                            MultipassdGovernor, "on_monitor_exit", new_callable=AsyncMock
                        ),
                    ):
                        with conftest.multipassd_impl():
                            Path({str(test_ready)!r}).write_text("ready")
                            while True:
                                time.sleep(0.05)
                """
            )
        )

        tests_dir = str(Path(__file__).resolve().parent.parent)
        env = os.environ.copy()
        existing_pythonpath = env.get("PYTHONPATH", "")
        env["PYTHONPATH"] = (
            f"{tests_dir}{os.pathsep}{existing_pythonpath}"
            if existing_pythonpath
            else tests_dir
        )

        pytest_log = tmp_path / "pytest_output.log"
        with open(pytest_log, "w", encoding="utf-8") as log_file:
            popen_kwargs = {
                "env": env,
                "stdout": log_file,
                "stderr": subprocess.STDOUT,
            }
            if sys.platform == "win32":
                popen_kwargs["creationflags"] = (
                    subprocess.CREATE_NEW_PROCESS_GROUP | subprocess.CREATE_NEW_CONSOLE
                )

            proc = subprocess.Popen(
                [sys.executable, "-m", "pytest", str(test_file), "-o", "addopts="],
                **popen_kwargs,
            )
            try:
                _wait_for_file(daemon_ready, proc=proc)
                _wait_for_file(test_ready, proc=proc)

                if sys.platform == "win32":
                    send_ctrl_c(proc.pid)
                else:
                    os.kill(proc.pid, signal.SIGINT)

                returncode = proc.wait(timeout=20)
            except Exception as exc:
                log_file.flush()
                raise RuntimeError(
                    f"{exc}\n--- pytest output ---\n{pytest_log.read_text(encoding='utf-8', errors='replace')}"
                ) from exc
            finally:
                if proc.poll() is None:
                    proc.kill()
                    proc.wait(timeout=5)

        assert returncode == int(pytest.ExitCode.INTERRUPTED)
        assert daemon_stopped.read_text() == "stopped"
        storage_dir = Path(storage_record.read_text())
        assert not storage_dir.exists()
