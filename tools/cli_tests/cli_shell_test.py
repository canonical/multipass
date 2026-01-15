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

"""Multipass command line tests for the `shell` command."""

import pytest

from cli_tests.multipass import shell, requires_multipass
from pexpect import EOF as pexpect_eof


@pytest.mark.shell
@pytest.mark.usefixtures("multipassd")
class TestShell:
    def test_shell(self, instance):
        """Launch an Ubuntu VM. Then, try to shell into it and execute some
        basic commands."""

        with shell(instance) as vm_shell:
            vm_shell.expect(r"ubuntu@.*:.*\$", timeout=30)

            # Send a command and expect output
            vm_shell.sendline('echo "Hello from multipass"')
            vm_shell.expect("Hello from multipass")
            vm_shell.expect(r"ubuntu@.*:.*\$")

            # Test another command
            vm_shell.sendline("pwd")
            vm_shell.expect(r"/home/ubuntu")
            vm_shell.expect(r"ubuntu@.*:.*\$")

            # Core count
            vm_shell.sendline("nproc")
            vm_shell.expect("2")
            vm_shell.expect(r"ubuntu@.*:.*\$", timeout=30)

            # User name
            vm_shell.sendline("whoami")
            vm_shell.expect("ubuntu")
            vm_shell.expect(r"ubuntu@.*:.*\$", timeout=30)

            # Hostname
            vm_shell.sendline("hostname")
            vm_shell.expect(f"{instance}")
            vm_shell.expect(r"ubuntu@.*:.*\$", timeout=30)

            # Ubuntu Series
            vm_shell.sendline("grep --color=never '^VERSION=' /etc/os-release")
            vm_shell.expect(r'VERSION="24\.04\..* LTS \(Noble Numbat\)"')
            vm_shell.expect(r"ubuntu@.*:.*\$", timeout=30)

            # Exit the shell
            vm_shell.sendline("exit")
            vm_shell.expect(pexpect_eof)
            vm_shell.wait()

            assert vm_shell.exitstatus == 0

    @requires_multipass(">=1.17.0")
    def test_shell_exit_code_propagation(self, instance):
        """Launch an Ubuntu VM. Then, try to shell into it and execute some
        basic commands."""

        with shell(instance) as vm_shell:
            vm_shell.expect(r"ubuntu@.*:.*\$", timeout=30)

            # Send a command and expect output
            vm_shell.sendline('exit 42')
            vm_shell.expect(pexpect_eof)
            vm_shell.wait()

            assert vm_shell.exitstatus == 42
