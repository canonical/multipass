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

"""Pytest customizations."""

import shutil
import os
import re
import sys
import subprocess
import time
import select
import threading

import pexpect

from cli_tests.utils import multipass, get_sudo_tool, die


class MultipassdController:
    def __init__(self, build_root, data_root, print_daemon_output=False):
        self.build_root = build_root
        self.data_root = data_root
        self.multipassd_path = shutil.which("multipassd", path=self.build_root)
        self.print_daemon_output = print_daemon_output
        if not self.multipassd_path:
            die(11, "Could not find 'multipassd' executable!")

        self.multipassd_env = os.environ.copy()
        self.multipassd_env["MULTIPASS_STORAGE"] = self.data_root

        self.multipassd_proc = None
        self.monitor_thread = None
        self.monitoring = False
        self.graceful_exit = False

    def _monitor(self):
        poll_interval = 0.5

        def read_remaining_stdout():
            """Use communicate to get all remaining output"""
            if not self.print_daemon_output:
                return
            try:
                stdout_data, _ = self.multipassd_proc.communicate(timeout=5)
                if stdout_data:
                    print(stdout_data, end="")
            except subprocess.TimeoutExpired:
                pass

        def stdout_plumbing():
            if not self.print_daemon_output:
                return
            # Check if data is available (non-blocking)
            rlist, _, _ = select.select([self.multipassd_proc.stdout], [], [], 0)
            if rlist:
                # Read all available lines at once
                try:
                    while True:
                        # Try to read without blocking
                        rlist, _, _ = select.select(
                            [self.multipassd_proc.stdout], [], [], 0
                        )
                        if not rlist:
                            break
                        line = self.multipassd_proc.stdout.readline()
                        if not line:
                            break
                        print(line, end="")
                except (BlockingIOError, IOError):
                    pass

        while self.monitoring:

            stdout_plumbing()

            if self.multipassd_proc.poll() is not None:
                # Graceful exit.
                if self.graceful_exit and self.multipassd_proc.returncode == 0:
                    read_remaining_stdout()
                    return

                reason_dict = {
                    r".*dnsmasq: failed to create listening socket.*": "Could not bind dnsmasq to port 53, is there another process running?",
                    r'.*Failed to get shared "write" lock': "Cannot open a image file for writing, is another process holding a write lock?",
                }
                reasons = []

                for line in self.multipassd_proc.stdout:
                    if self.print_daemon_output:
                        print(line, end="")
                    for k, v in reason_dict.items():
                        if re.search(k, line):
                            reasons.append(f"\nReason: {v}")
                reasons = "\n".join(reasons)
                die(
                    12,
                    f"FATAL: multipassd died with code {self.multipassd_proc.returncode}!\n{reasons}",
                )
            time.sleep(poll_interval)
        read_remaining_stdout()

    def start_monitoring(self):
        self.monitoring = True
        self.monitor_thread = threading.Thread(target=self._monitor, daemon=True)
        self.monitor_thread.start()

    def stop_monitoring(self):
        self.monitoring = False
        self.monitor_thread.join(timeout=10)
        self.monitor_thread = None

    @staticmethod
    def wait_for_multipassd_ready(timeout=60):
        """
        Wait until Multipass daemon starts responding to the CLI commands.
        For that, the function tries to shell into a non-existent instance.
        The daemon is expected to respond with "instance does not exists"
        when it's up and running.

        When the daemon starts, the function also verifies that the daemon's
        version matches the CLI's version.

        If any of these expectancies fail, it's a hard failure and it'll cause
        the test executable to exit with an error code.
        """
        deadline = time.time() + timeout
        while time.time() < deadline:
            # The function uses the `find` command to check if daemon is ready.`
            try:
                if not multipass("find", timeout=10):
                    raise ConnectionError()

                with multipass("version") as version:
                    version_out = (
                        version.content.splitlines() if version.content else []
                    )
                    # One line for multipass, one line for multipassd.
                    if len(version_out) == 2:
                        cli_ver = version_out[0].split()[-1]
                        daemon_ver = version_out[1].split()[-1]
                        if cli_ver == daemon_ver:
                            return True  # No issues.

                        sys.stderr.write(
                            f"Version mismatch detected!\n"
                            f"👉 CLI version: {cli_ver}\n"
                            f"👉 Daemon version: {daemon_ver}\n"
                        )
                        sys.stderr.flush()
                        return False
            except pexpect.exceptions.EOF:
                pass
            except pexpect.exceptions.TIMEOUT:
                pass
            except ConnectionError:
                pass
            time.sleep(0.2)

        sys.stderr.write("\n❌ Fatal error: The daemon did not respond in time!\n")
        sys.stderr.flush()
        return False

    def _terminate_multipassd(self):
        self.multipassd_proc.terminate()  # polite SIGTERM
        try:
            self.multipassd_proc.wait(timeout=10)
        except subprocess.TimeoutExpired:
            print("⏰ Graceful shutdown failed, killing forcefully...")
            self.multipassd_proc.kill()  # no mercy
            self.multipassd_proc.wait()  # ensure it's reaped
        finally:
            # self.multipassd_proc = None
            sys.stderr.flush()
            sys.stdout.flush()

    def restart(self):
        self.stop()
        self.start()

    def stop(self):
        self.graceful_exit = True
        self._terminate_multipassd()
        self.stop_monitoring()

    def start(self):
        prologue = []
        if sys.platform == "win32":
            pass
        else:
            prologue = [get_sudo_tool(), "env", f"MULTIPASS_STORAGE={self.data_root}"]

        self.multipassd_proc = subprocess.Popen(
            prologue + [self.multipassd_path, "--logger=stderr"],
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            env = self.multipassd_env
        )
        self.start_monitoring()
        if not MultipassdController.wait_for_multipassd_ready():
            print("⚠️ multipassd not ready, attempting graceful shutdown...")
            self._terminate_multipassd()
            die(12, "Tests cannot proceed, daemon not responding in time.")
