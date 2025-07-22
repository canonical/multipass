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

"""Multipass command line tests for the `mount` feature."""

import shutil
from pathlib import Path

import pytest

from cli_tests.utils import multipass, TempDirectory, mounts


@pytest.mark.mount
class TestMount:
    """Virtual machine mount tests."""

    # TODO: "native"
    @pytest.mark.parametrize("mount_type", ["classic"])
    def test_mount(self, instance, mount_type):

        with TempDirectory() as mount_dir:
            if mount_type == "native":
                assert multipass("stop", instance)

            assert multipass("mount", "--type", mount_type, str(mount_dir), instance)
            assert multipass("start", instance)

            assert mounts(instance) == {
                f"/home/ubuntu/{mount_dir.name}": {
                    "gid_mappings": ["1000:default"],
                    "source_path": str(mount_dir),
                    "uid_mappings": ["1000:default"],
                }
            }
            assert multipass(
                "exec", instance, "--", "ls", f"/home/ubuntu/{mount_dir.name}"
            )
            assert multipass("umount", instance)
            assert mounts(instance) == {}

    @pytest.mark.parametrize("mount_type", ["classic"])
    def test_mount_modify(self, instance, mount_type):

        with TempDirectory() as mount_dir:
            if mount_type == "native":
                assert multipass("stop", instance)

            assert multipass("mount", "--type", mount_type, str(mount_dir), instance)
            assert multipass("start", instance)

            assert mounts(instance) == {
                f"/home/ubuntu/{mount_dir.name}": {
                    "gid_mappings": ["1000:default"],
                    "source_path": str(mount_dir),
                    "uid_mappings": ["1000:default"],
                }
            }

            instance_target_path = Path("/home") / "ubuntu" / mount_dir.name
            subdir = Path("subdir1") / "subdir2" / "subdir3"

            assert multipass("exec", instance, "--", "ls", str(instance_target_path))

            assert multipass(
                "exec",
                instance,
                "--",
                f'bash -c \'echo "hello there" > {str(instance_target_path)}/file1.txt',
            )

            assert multipass(
                "exec",
                instance,
                "--",
                f"mkdir -p {str(instance_target_path / subdir)}",
            )

            assert multipass(
                "exec",
                instance,
                "--",
                f'bash -c \'echo "hello there" > {str(instance_target_path / subdir)}/file2.txt',
            )

            expected_subdir = mount_dir / subdir
            expected_file1 = mount_dir / "file1.txt"
            expected_file2 = expected_subdir / "file2.txt"
            assert expected_file1.read_text() == "hello there\n"
            assert expected_file2.read_text() == "hello there\n"

            expected_file1.unlink()
            assert not expected_file1.exists()
            expected_file2.unlink()
            assert not expected_file2.exists()
            expected_subdir.rmdir()
            assert not expected_subdir.exists()

            assert multipass("umount", instance)
            assert mounts(instance) == {}
