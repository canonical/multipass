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

"""Multipass command line tests for the `multipass launch`."""

import pytest

from cli_tests.multipass import exec


@pytest.mark.exec
@pytest.mark.usefixtures("multipassd")
class TestExec:
    """CLI VM exec tests."""

    def test_exec_propagates_command_exit_code(self, instance):
        """Check if exec propagates exit code of the command executed in the VM"""
        assert exec(instance, "exit", "33").exitstatus == 33
