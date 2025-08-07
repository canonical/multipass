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

"""Multipass command line tests for the transfer CLI command."""

import pytest

from cli_tests.utilities import TempDirectory
from cli_tests.multipass import multipass, file_exists, read_file


@pytest.mark.transfer
@pytest.mark.usefixtures("multipassd_class_scoped")
class TestTransfer:
    """Transfer command tests."""

    def test_transfer_single_file(self, instance):
        """Try to list instances whilst there are none."""
        with TempDirectory() as tmp:
            file = tmp / "testfile.txt"
            file.touch()
            file.write_text("hello from the other side")

            # Push
            assert multipass("transfer", str(file), f"{instance}:.")
            assert file_exists(instance, "testfile.txt")

            # Pull
            pull_file = tmp / "testfile_pull.txt"
            assert not pull_file.exists()
            assert multipass("transfer", f"{instance}:testfile.txt", str(pull_file))
            assert pull_file.exists()
            assert pull_file.read_text() == "hello from the other side"

    def test_transfer_single_file_create_parents(self, instance):
        """Try to list instances whilst there are none."""
        with TempDirectory() as tmp:
            file = tmp / "testfile.txt"
            file.touch()
            file.write_text("hello from the other side")

            # Push
            assert not multipass(
                "transfer", str(file), f"{instance}:bar/baz/testfile.txt"
            )
            # This fails.
            # assert multipass("transfer", "--parents", str(file), f"{instance}:bar/baz/")
            assert multipass(
                "transfer", "--parents", str(file), f"{instance}:bar/baz/testfile.txt"
            )
            assert file_exists(instance, "bar/baz/testfile.txt")

            # This must fail, but it does not.
            # assert not multipass(
            #     "transfer", f"{instance}:bar/baz/testfile.txt", "bar/baz/testfile.txt"
            # )

            pull_file = tmp / "bar" / "baz" / "testfile.txt"
            assert multipass(
                "transfer",
                "--parents",
                f"{instance}:bar/baz/testfile.txt",
                str(pull_file),
            )
            assert pull_file.exists()
            assert pull_file.read_text() == "hello from the other side"

    def test_transfer_directory_recursive(self, instance):
        """Try to list instances whilst there are none."""
        with TempDirectory() as tmp:
            (tmp / "bar").mkdir()
            file1 = tmp / "testfile1.txt"
            file1.touch()
            file1.write_text("hello from the other side")
            file2 = tmp / "bar" / "testfile2.txt"
            file2.touch()
            file2.write_text("i must've called a thousand times")

            # Push
            assert not multipass("transfer", str(tmp), f"{instance}:dest")
            assert multipass("transfer", "--recursive", str(tmp), f"{instance}:dest")
            assert file_exists(instance, "dest/testfile1.txt")
            assert file_exists(instance, "dest/bar/testfile2.txt")
            assert (
                read_file(instance, "dest/testfile1.txt") == "hello from the other side"
            )
            assert (
                read_file(instance, "dest/bar/testfile2.txt")
                == "i must've called a thousand times"
            )

            # Pull
            pull_folder = tmp / "dest"
            pull_file1 = pull_folder / "testfile1.txt"
            pull_file2 = pull_folder / "bar" / "testfile2.txt"
            assert multipass("transfer", "--recursive", f"{instance}:dest", pull_folder)
            assert pull_file1.exists()
            assert pull_file2.exists()
            assert pull_file1.read_text() == "hello from the other side"
            assert pull_file2.read_text() == "i must've called a thousand times"
