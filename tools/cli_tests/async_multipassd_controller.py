#!/usr/bin/env python3

"""Asyncio-based Multipassd Controller"""

import asyncio
import shutil
import os
import re
import sys
import subprocess
import time
import logging

import pexpect

from cli_tests.utils import multipass, get_sudo_tool, die, get_multipass_env, authenticate_client_cert, get_multipassd_path, get_multipass_path, run_as_privileged, get_client_cert_path
from cli_tests.config import config

def send_ctrl_c(pid):
    subprocess.run(
    f'python -c "import ctypes; k=ctypes.windll.kernel32; k.FreeConsole(); k.AttachConsole({pid}); k.GenerateConsoleCtrlEvent(0, 0)"',
        shell=False,
    )


class AsyncMultipassdController:
    def __init__(self, asyncio_loop, build_root, data_root, print_daemon_output=True):
        if not get_multipassd_path():
            die(11, "Could not find 'multipassd' executable!")

        self.asyncio_loop = asyncio_loop
        self.build_root = build_root
        self.data_root = data_root
        self.print_daemon_output = print_daemon_output
        self.multipassd_proc = None
        self.monitor_task = None
        self.graceful_exit_initiated = False
        self.autorestart_attempts = 0

    async def _read_stream(self, stream):
        """Read from stream line by line and print if enabled"""
        while True:
            try:
                line = await stream.readline()
                if not line:
                    break

                line = line.decode('utf-8', errors='replace')
                if self.print_daemon_output:
                    print(line, end='')

                # Check for known error patterns
                for pattern, reason in self._get_error_patterns().items():
                    if re.search(pattern, line):
                        return reason

            except Exception as e:
                if self.print_daemon_output:
                    print(f"Error reading stream: {e}", file=sys.stderr)
                break
        return None

    def _get_error_patterns(self):
        return {
            r".*dnsmasq: failed to create listening socket.*":
                "Could not bind dnsmasq to port 53, is there another process running?",
            r'.*Failed to get shared "write" lock':
                "Cannot open a image file for writing, is another process holding a write lock?",
        }

    async def _monitor(self):
        """Monitor the daemon process"""
        error_reasons = []

        # Create tasks to read both stdout and stderr
        stdout_task = asyncio.create_task(self._read_stream(self.multipassd_proc.stdout))

        try:
            # Wait for process to exit
            returncode = await self.multipassd_proc.wait()

            # Wait for stream reading to complete
            error_reason = await stdout_task
            if error_reason:
                error_reasons.append(f"\nReason: {error_reason}")

            multipassd_settings_changed_code = 42

            # Daemon exited due to settings change. The controller should restart it.
            if returncode == multipassd_settings_changed_code:
                self.autorestart_attempts = self.autorestart_attempts + 1
                return

            # Check exit status
            if (self.graceful_exit_initiated or self.autorestart_attempts > 0) and returncode == 0:
                return  # Normal exit

            # Abnormal exit
            reasons = "\n".join(error_reasons) if error_reasons else ""
            die(12, f"FATAL: multipassd died with code {returncode}!{reasons}")

        except asyncio.CancelledError:
            # Task was cancelled, this is expected during shutdown
            stdout_task.cancel()
            raise

    def _on_daemon_exit(self, task):
        self.graceful_exit_initiated = False
        self.multipassd_proc = None
        self.monitor_task = None
        logging.info("daemon exit")

        if self.autorestart_attempts > 0:
            logging.info(f"attempting daemon autorestart, attempts: {self.autorestart_attempts}")
            self.autorestart_attempts = self.autorestart_attempts - 1
            self.asyncio_loop.run_fn(lambda: asyncio.create_task(self.start()))

    async def start(self):
        """Start the multipassd daemon"""

        prologue = []
        creationflags = None
        if sys.platform == "win32":
            # No prologue needed -- just pass it via process env
            creationflags = subprocess.CREATE_NEW_PROCESS_GROUP | subprocess.CREATE_NEW_CONSOLE
        else:
            prologue = [*get_sudo_tool(), "env", f"MULTIPASS_STORAGE={self.data_root}"]

        # Call multipass cli to create the client certs
        version_proc = await asyncio.create_subprocess_exec(
                            get_multipass_path(), "version",
                            stdout=asyncio.subprocess.PIPE,
                            stderr=asyncio.subprocess.STDOUT,
                            env=get_multipass_env()
                        )
        await asyncio.wait_for(version_proc.wait(), timeout=10)

        # It's important that we get the cert path here instead of under
        # run_as_privileged.
        cert_path = get_client_cert_path()
        # Authenticate the client against the daemon
        run_as_privileged(authenticate_client_cert, str(cert_path), config.data_root)

        # Create subprocess. The child process needs a new process group and a
        # new console to properly receive the Ctrl-C signal without interfering
        # with the test process itself.
        self.multipassd_proc = await asyncio.create_subprocess_exec(
            *prologue, get_multipassd_path(), "--logger=stderr", "--verbosity=trace",
            stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.STDOUT,
            env=get_multipass_env(),
            **({"creationflags": creationflags} if creationflags is not None else {})
        )

        # Start monitoring task
        self.monitor_task = asyncio.create_task(self._monitor())
        self.monitor_task.add_done_callback(self._on_daemon_exit)

        await asyncio.sleep(0)  # Yield control

        # Wait for daemon to be ready
        if not await self.wait_for_multipassd_ready():
            print("⚠️ multipassd not ready, attempting graceful shutdown...")
            await self._terminate_multipassd()
            die(12, "Tests cannot proceed, daemon not responding in time.")

    async def stop(self):
        """Stop the multipassd daemon"""
        self.graceful_exit_initiated = True
        await self._terminate_multipassd()
        # Wait for the monitor task instead?
        if self.monitor_task:
            try:
                await asyncio.wait_for(self.monitor_task, timeout=10)
            except asyncio.TimeoutError:
                print("⚠️ monitor_task still not done, cancelling it...")
                self.monitor_task.cancel()
                try:
                    await self.monitor_task
                except asyncio.CancelledError:
                    print("🪓 monitor_task cancelled")
            except Exception as e:
                print(f"💥 monitor_task failed: {e}")

    async def restart_async(self):
        await self.stop()
        await self.start()

    def restart(self, timeout=60):
        """Restart the daemon"""
        self.asyncio_loop.run(self.restart_async()).result(timeout=timeout)


    def autorestart(self, times=1):
        self.autorestart_attempts = times

    async def _terminate_multipassd(self):
        """Terminate the multipassd process"""
        if not self.multipassd_proc:
            return

        if sys.platform == "win32":
            # We need to send ctrl-c keystroke here because Windows has no
            # notion of "signals" as in POSIX.
            send_ctrl_c(self.multipassd_proc.pid)
        else:
            self.multipassd_proc.terminate()  # SIGTERM

        try:
            # Wait for graceful shutdown
            await asyncio.wait_for(self.multipassd_proc.wait(), timeout=20)
        except asyncio.TimeoutError:
            print("⏰ Graceful shutdown failed, killing forcefully...")
            self.multipassd_proc.kill()  # SIGKILL
            await self.multipassd_proc.wait()
        finally:
            sys.stderr.flush()
            sys.stdout.flush()

    @staticmethod
    async def wait_for_multipassd_ready(timeout=60):
        """
        Wait until Multipass daemon starts responding to CLI commands.
        """
        deadline = time.time() + timeout

        while time.time() < deadline:
            try:
                # Check if daemon responds to find command
                proc = await asyncio.create_subprocess_exec(
                    get_multipass_path(), "find",
                    stdout=asyncio.subprocess.PIPE,
                    stderr=asyncio.subprocess.STDOUT,
                    env=get_multipass_env()
                )

                try:
                    await asyncio.wait_for(proc.wait(), timeout=10)
                    if proc.returncode == 0:
                        # Check version match
                        version_proc = await asyncio.create_subprocess_exec(
                            get_multipass_path(), "version",
                            stdout=asyncio.subprocess.PIPE,
                            stderr=asyncio.subprocess.STDOUT,
                            env=get_multipass_env()
                        )

                        stdout, _ = await version_proc.communicate()
                        version_lines = stdout.decode().strip().splitlines()

                        if len(version_lines) == 2:
                            cli_ver = version_lines[0].split()[-1]
                            daemon_ver = version_lines[1].split()[-1]

                            if cli_ver == daemon_ver:
                                return True

                            sys.stderr.write(
                                f"Version mismatch detected!\n"
                                f"👉 CLI version: {cli_ver}\n"
                                f"👉 Daemon version: {daemon_ver}\n"
                            )
                            sys.stderr.flush()
                            return False

                    else:
                        stdout, _ = await proc.communicate()
                        logging.debug(stdout)

                except asyncio.TimeoutError:
                    pass

            except Exception as exc:
                logging.error(exc)

            await asyncio.sleep(0.2)

        sys.stderr.write("\n❌ Fatal error: The daemon did not respond in time!\n")
        sys.stderr.flush()
        return False