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

"""Tests for configuring the daemon gRPC port in the Windows registry."""

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


def configured_daemon_port():
    import winreg

    try:
        with winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, multipass_registry_key) as key:
            port, value_type = winreg.QueryValueEx(key, multipass_registry_value)
    except OSError:
        pytest.skip(
            f"The registry value '{multipass_registry_value}' is not configured"
        )

    if value_type != winreg.REG_DWORD or not 0 < port < 65536:
        pytest.skip("The configured daemon port is not a valid REG_DWORD port")

    return port


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
        state = service_state()
        if state == expected:
            return True

        if expected == "RUNNING" and state == "STOPPED":
            # Restart along the way in case the service's own failure actions
            # do not kick in quickly enough.
            subprocess.run(
                [*sudo("sc.exe", "start", multipass_service)],
                capture_output=True,
                check=False,
            )

        time.sleep(1)
    return False


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


def restore_registry_port(port):
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
    assert result.returncode == 0, f"could not restore the daemon port: {result.stderr}"


class TestDaemonPort:
    """Daemon port configuration tests."""

    def test_daemon_listens_on_configured_port(self):
        require_windows_service()
        port = configured_daemon_port()

        assert wait_for_state("RUNNING"), "the Multipass service is not running"
        assert wait_for_port_open(port), f"nothing is listening on port {port}"

        with multipass("version", retry=3) as output:
            assert output
            assert "multipassd" in output.content

    def test_daemon_moves_away_when_configured_port_taken(self):
        require_windows_service()
        original_port = configured_daemon_port()

        stop_service()

        with port_occupied(original_port):
            subprocess.run(
                [*sudo("sc.exe", "start", multipass_service)],
                capture_output=True,
                check=False,
            )

            # Instead of getting stuck, the daemon must move to another port,
            # persist it in the registry and come up, so that clients
            # (which read the same setting) find it there.
            assert wait_for_state("RUNNING", timeout=120), (
                "the service did not come up even though it could move to a free port"
            )

            new_port = configured_daemon_port()
            assert new_port != original_port, "the daemon did not move away"
            assert wait_for_port_open(new_port), (
                f"the daemon is not listening on the new port {new_port}"
            )

        # The move must be crystal clear in the event log, naming the
        # offending process.
        assert wait_for_event(
            (
                f"configured port {original_port} is not available",
                "is already in use by",
                "PID",
            )
        ), "the daemon port move did not show up in the event log"

        with multipass("version", retry=3) as output:
            assert output
            assert "multipassd" in output.content

        # Restore the original configuration for the rest of the suite.
        restore_registry_port(original_port)
        stop_service()
        subprocess.run(
            [*sudo("sc.exe", "start", multipass_service)],
            capture_output=True,
            check=False,
        )
        assert wait_for_state("RUNNING"), "the service did not come back up"
        assert wait_for_port_open(original_port), (
            f"the daemon did not listen on port {original_port} again"
        )
