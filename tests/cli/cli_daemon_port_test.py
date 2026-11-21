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

"""Tests for the daemon gRPC port setting in the Windows registry."""

import re
import socket
import subprocess
import sys
import time
from contextlib import contextmanager

import pytest

from cli.config import cfg
from cli.multipass import multipass
from cli.utilities import sudo

multipass_service = "Multipass"
multipass_registry_key = r"SOFTWARE\Canonical\Multipass"
multipass_registry_value = "ServerPort"

pytestmark = [
    pytest.mark.daemon_port,
    pytest.mark.skipif(sys.platform != "win32", reason="Windows-only tests"),
]


def require_windows_service():
    if cfg.daemon_controller != "winsvc":
        pytest.skip("These tests require the Windows service daemon controller")


def service_state():
    result = subprocess.run(
        ["sc.exe", "query", multipass_service],
        capture_output=True,
        text=True,
        check=False,
    )
    match = re.search(r"STATE\s*:\s*\d+\s+(\w+)", result.stdout)
    return match.group(1) if match else None


def wait_for_state(expected, timeout=120):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if service_state() == expected:
            return True
        time.sleep(1)
    return False


def start_service():
    subprocess.run(
        [*sudo("sc.exe", "start", multipass_service)],
        capture_output=True,
        check=False,
    )


def stop_service():
    subprocess.run(
        [*sudo("sc.exe", "stop", multipass_service)],
        capture_output=True,
        check=False,
    )
    assert wait_for_state("STOPPED"), "could not stop the Multipass service"


def wait_for_port_open(port, timeout=30):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        try:
            with socket.create_connection(("127.0.0.1", port), timeout=5):
                return True
        except OSError:
            time.sleep(1)
    return False


@contextmanager
def port_occupied(port):
    """Occupy the port on loopback, like a foreign application would."""
    sockets = []

    for family, address in (
        (socket.AF_INET, ("127.0.0.1", port)),
        (socket.AF_INET6, ("::1", port)),
    ):
        sock = socket.socket(family, socket.SOCK_STREAM)
        try:
            sock.bind(address)
            sock.listen(1)
            sockets.append(sock)
        except OSError:
            sock.close()  # e.g. no IPv6 loopback available

    assert sockets, f"could not occupy port {port}"
    try:
        yield
    finally:
        for sock in sockets:
            sock.close()


@contextmanager
def seeded_registry_port():
    """Seed a free port in the registry, restoring the previous state after."""
    import winreg

    probe = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    probe.bind(("127.0.0.1", 0))
    port = probe.getsockname()[1]
    probe.close()

    try:
        with winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, multipass_registry_key) as key:
            old_port, _ = winreg.QueryValueEx(key, multipass_registry_value)
    except OSError:
        old_port = None

    result = subprocess.run(
        [
            *sudo(
                "reg",
                "add",
                f"HKLM\\{multipass_registry_key}",
                "/v",
                multipass_registry_value,
                "/t",
                "REG_DWORD",
                "/d",
                str(port),
                "/f",
            )
        ],
        capture_output=True,
        text=True,
        check=False,
    )
    assert result.returncode == 0, f"could not seed the daemon port: {result.stderr}"

    try:
        yield port
    finally:
        if old_port is None:
            subprocess.run(
                [
                    *sudo(
                        "reg",
                        "delete",
                        f"HKLM\\{multipass_registry_key}",
                        "/v",
                        multipass_registry_value,
                        "/f",
                    )
                ],
                capture_output=True,
                check=False,
            )
        else:
            subprocess.run(
                [
                    *sudo(
                        "reg",
                        "add",
                        f"HKLM\\{multipass_registry_key}",
                        "/v",
                        multipass_registry_value,
                        "/t",
                        "REG_DWORD",
                        "/d",
                        str(old_port),
                        "/f",
                    )
                ],
                capture_output=True,
                check=False,
            )

        # leave the daemon running on the restored setting
        stop_service()
        start_service()
        assert wait_for_state("RUNNING"), "the service did not come back up"


def recent_multipass_events(count=20):
    result = subprocess.run(
        [
            *sudo(
                "wevtutil",
                "qe",
                "Application",
                "/q:*[System[Provider[@Name='Multipass']]]",
                f"/c:{count}",
                "/rd:true",
                "/f:text",
            )
        ],
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        check=False,
    )
    assert result.returncode == 0, f"could not read the event log: {result.stderr}"
    return result.stdout


def wait_for_event(expected_snippets, timeout=90):
    """Wait for the daemon's port-related messages to land in the event log."""
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        events = recent_multipass_events()
        missing = [snippet for snippet in expected_snippets if snippet not in events]
        if not missing:
            return True
        time.sleep(2)
    return False


class TestDaemonPort:
    """Daemon port setting tests."""

    def test_daemon_listens_on_configured_port(self):
        require_windows_service()
        with seeded_registry_port() as port:
            stop_service()
            start_service()

            assert wait_for_state("RUNNING"), "the Multipass service is not running"
            assert wait_for_port_open(port), f"nothing is listening on port {port}"

            with multipass("version", retry=3) as output:
                assert output
                assert "multipassd" in output.content

    def test_daemon_fails_fast_with_clear_error_when_port_taken(self):
        require_windows_service()
        with seeded_registry_port() as port:
            stop_service()

            with port_occupied(port):
                start_service()

                # Instead of getting stuck or failing obscurely, the daemon
                # must fail fast, with a clear error naming the offending
                # process and the way out.
                assert wait_for_event(
                    (
                        "cannot listen on",
                        f"port {port}",
                        "is already in use by",
                        "PID",
                        "reg add",
                    )
                ), "the daemon port-conflict error did not show up in the event log"

            # Once the port is free again, the service must come back and work.
            start_service()
            assert wait_for_state("RUNNING"), "the service did not recover"
            assert wait_for_port_open(port), f"the daemon did not listen on port {port} again"

            with multipass("version", retry=3) as output:
                assert output
                assert "multipassd" in output.content
