#!/usr/bin/env python3

"""Asyncio-based Multipassd Controller"""

import asyncio
import logging
import re
import sys
import time
from contextlib import suppress
from typing import Optional

import pytest
from pytest import Session

from cli_tests.config import config
from cli_tests.multipass import (
    authenticate_client_cert,
    get_client_cert_path,
    get_multipass_env,
    get_multipass_path,
    TestSessionFailure,
    TestCaseFailure
)
from cli_tests.utilities import (
    BooleanLatch,
    SilentAsyncSubprocess,
    StdoutAsyncSubprocess,
    run_in_new_interpreter,
)

from .multipassd_controller import MultipassdController


class MultipassdGovernor:
    """
    Orchestrates the lifecycle of the **multipassd** daemon during tests.
    """

    def __init__(
        self, controller: MultipassdController, asyncio_loop, print_daemon_output=True
    ):
        self.asyncio_loop = asyncio_loop
        self.print_daemon_output = print_daemon_output
        self.controller = controller
        self.daemon_ready_event = BooleanLatch()
        self.monitor_task = None
        self._reset_state()

    def _reset_state(self):
        self.monitor_task = None
        self.graceful_exit_initiated = False
        self.daemon_ready_event.clear()

    def _get_error_patterns(self):
        return {
            r".*dnsmasq: failed to create listening socket.*": "Could not bind dnsmasq to port 53, is there another process running?",
            r'.*Failed to get shared "write" lock.*': "Cannot open an image file for writing, is another process holding a write lock?",
            r".*Only one usage of each socket address.*": "Could not bind gRPC port -- is there another daemon process running?",
        }

    async def _read_stream(self):
        """Read from stream line by line and print if enabled"""
        logging.debug("multipassd-governor :: read_stream start")
        try:
            async for line in self.controller.follow_output():
                if self.print_daemon_output:
                    print(line, end="")

                for pattern, reason in self._get_error_patterns().items():
                    if re.search(pattern, line):
                        return reason

        except asyncio.CancelledError:
            # Handle it gracefully.
            pass
        except Exception as e:
            if self.print_daemon_output:
                print(f"Error in log reader: {e}", file=sys.stderr)
        logging.debug("multipassd-governor :: read_stream finish")

    async def _monitor(self):
        """Monitor the daemon process"""
        error_reasons = []

        # Create tasks to read both stdout and stderr
        stdout_task = asyncio.create_task(self._read_stream())

        try:
            # Wait for process to exit
            returncode = await self.controller.wait_exit()

            # Wait for stream reading to complete.
            if not stdout_task.done():
                stdout_task.cancel()

            error_reason = await stdout_task
            if error_reason:
                error_reasons.append(
                    f"\nReason: {error_reason}")

            # Check exit status
            if (self.graceful_exit_initiated and returncode == 0) or returncode == 42:
                return  # Normal exit

            if returncode is None:
                # Unable to determine the exit code.
                logging.warning(
                    "multipassd-governor :: monitor task -- unable to determine daemon exit code")
                return

            # Abnormal exit
            reasons = "\n".join(error_reasons) if error_reasons else ""

            # If the daemon has exited with a failure, subsequent attempts are most likely
            # to fail. Hence, abort the whole session.
            if returncode == 1:
                raise TestSessionFailure(
                    f"FATAL: multipassd died with code {returncode}!{reasons}"
                )
            raise TestCaseFailure(
                f"FATAL: multipassd died with code {returncode}!{reasons}"
            )

        except asyncio.CancelledError:
            # Task was cancelled, this is expected during shutdown
            stdout_task.cancel()
            raise

    async def on_monitor_exit(self, task):
        logging.debug(
            f"multipassd-governor :: monitor task exited (cancelled: {task.cancelled()})"
        )
        if task.cancelled():
            return

        multipassd_settings_changed_code = 42
        daemon_exit_code = await self.controller.exit_code()
        needs_restarting = (
            not self.controller.supports_self_autorestart()
            and daemon_exit_code == multipassd_settings_changed_code
        )
        self._reset_state()
        if needs_restarting:
            logging.debug("multipassd-governor :: daemon auto-restarting")
            self.asyncio_loop.run_fn(
                lambda: asyncio.create_task(self.start_async()))

    async def _ensure_client_certs_are_created(self):
        # Call multipass cli to create the client certs
        async with SilentAsyncSubprocess(
            get_multipass_path(),
            "version",
            env=get_multipass_env(),
        ) as version_proc:
            await asyncio.wait_for(version_proc.wait(), timeout=10)

    def _authenticate_client_cert(self):
        # It's important that we get the cert path here instead of under
        # run_as_privileged.
        cert_path = get_client_cert_path()
        # Authenticate the client against the daemon
        run_in_new_interpreter(
            authenticate_client_cert, str(cert_path), config.data_dir, privileged=True
        )

    async def start_async(self):
        """Start the multipassd daemon"""
        logging.debug("multipassd-governor :: start called")

        def _cancel(t: Optional[asyncio.Task]):
            if t and not t.done():
                t.cancel()

        async def _drain(t: Optional[asyncio.Task]):
            if t:
                with suppress(asyncio.CancelledError):
                    await t

        await self._ensure_client_certs_are_created()
        self._authenticate_client_cert()

        await self.controller.start()

        # Start monitoring task
        monitor_task = asyncio.create_task(
            self._monitor(), name="monitor_task")
        monitor_task.add_done_callback(
            lambda t: asyncio.create_task(self.on_monitor_exit(t))
        )

        ready_task = asyncio.create_task(
            self.wait_for_multipassd_ready(), name="wait_for_ready_task"
        )

        done, _ = await asyncio.wait(
            {ready_task, monitor_task},
            return_when=asyncio.FIRST_COMPLETED,
        )

        if monitor_task in done:
            _cancel(ready_task)
            await _drain(ready_task)
            try:
                await monitor_task
                asyncio.current_task().cancel()
                await asyncio.sleep(0)
            except TestSessionFailure as ex:
                Session.shouldfail = str(ex)
                raise

        # Wait for daemon to be ready
        if not await ready_task:
            logging.warning(
                "⚠️ multipassd not ready, attempting graceful shutdown...")
            await self.controller.stop()
            pytest.exit(
                "Tests cannot proceed, multipassd not responding in time.",
                returncode=12,
            )

        self.monitor_task = monitor_task
        self.daemon_ready_event.set()

    async def stop_async(self):
        """Stop the multipassd daemon"""
        logging.debug("multipassd-governor :: stop called")
        self.graceful_exit_initiated = True
        await self.controller.stop()

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
        await self.stop_async()
        await self.start_async()

    def restart(self, timeout=60):
        """Restart the daemon"""
        self.asyncio_loop.run(self.restart_async()).result(timeout=timeout)

    def start(self, timeout=60):
        """Start the daemon"""
        self.asyncio_loop.run(self.start_async()).result(timeout=timeout)

    def stop(self, timeout=60):
        """Stop the daemon"""
        self.asyncio_loop.run(self.stop_async()).result(timeout=timeout)

    def wait_for_shutdown(self, timeout=60):
        self.daemon_ready_event.wait_until(False, timeout=timeout)

    def wait_for_start(self, timeout=60):
        self.daemon_ready_event.wait_until(True, timeout=timeout)

    def wait_for_restart(self, timeout=60):
        # Restart events are opaque to us when the controller supports
        # self auto-restart. We need to ensure that the daemon is ready, though.
        if self.controller.supports_self_autorestart():
            self.asyncio_loop.run(
                self.controller.wait_for_self_autorestart()).result()
            logging.debug(
                "multipassd-governor :: daemon auto-restart detected")
            # Wait until daemon is ready
            self.asyncio_loop.run(self.wait_for_multipassd_ready()).result()
            return
        self.wait_for_shutdown(timeout=timeout)
        self.wait_for_start(timeout=timeout)

    @staticmethod
    async def wait_for_multipassd_ready(timeout=60):
        """
        Wait until Multipass daemon starts responding to CLI commands.
        """
        deadline = time.time() + timeout

        while time.time() < deadline:
            try:
                find_stdout = None
                find_exitcode = 0
                async with StdoutAsyncSubprocess(
                    get_multipass_path(),
                    "find",
                    "noble",
                    env=get_multipass_env(),
                ) as find_proc:
                    find_stdout, _ = await find_proc.communicate()
                    find_exitcode = find_proc.returncode

                if find_exitcode != 0:
                    if (
                        b"The client is not authenticated with the Multipass service"
                        in find_stdout
                    ):
                        logging.error("Client is not authenticated.")
                        Session.shouldfail = "Client is not authenticated."
                        return False
                    logging.debug(find_stdout)

                # Otherwise, verify the version
                async with StdoutAsyncSubprocess(
                    get_multipass_path(),
                    "version",
                    env=get_multipass_env(),
                ) as version_proc:
                    stdout, _ = await version_proc.communicate()
                    version_lines = stdout.decode().strip().splitlines()

                    if len(version_lines) >= 2:
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

            except asyncio.TimeoutError:
                # Try again.
                continue
            except asyncio.CancelledError:
                raise
            except Exception as exc:
                logging.error(exc)

            await asyncio.sleep(0.2)

        sys.stderr.write(
            "\n❌ Fatal error: The daemon did not respond in time!\n")
        sys.stderr.flush()
        return False
