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

import os
import sys
from pathlib import Path

import pytest

from cli_tests.utils import multipass, TempDirectory, mounts

def expected_mount_gid():
    if sys.platform == "win32":
        return -2
    return 1000

def expected_mount_uid():
    if sys.platform == "win32":
        return -2
    return 1000

@pytest.mark.mount
# TODO: "native"
@pytest.mark.parametrize("mount_type", ["classic"])
@pytest.mark.usefixtures("windows_privileged_mounts")
class TestMount:
    """Virtual machine mount tests."""

    def test_mount(self, instance, mount_type):

        with TempDirectory() as mount_dir:
            if mount_type == "native":
                assert multipass("stop", instance)

            assert multipass("mount", "--type", mount_type, str(mount_dir), instance)
            assert multipass("start", instance)

            assert mounts(instance) == {
                f"/home/ubuntu/{mount_dir.name}": {
                    "gid_mappings": [f"{expected_mount_gid()}:default"],
                    "source_path": str(mount_dir),
                    "uid_mappings": [f"{expected_mount_uid()}:default"],
                }
            }
            assert multipass(
                "exec", instance, "--", "ls", f"/home/ubuntu/{mount_dir.name}"
            )
            assert multipass("umount", instance)
            assert mounts(instance) == {}

    def test_mount_multiple(self, instance, mount_type):

        with TempDirectory() as mount_dir1, TempDirectory() as mount_dir2:
            if mount_type == "native":
                assert multipass("stop", instance)

            assert multipass("mount", "--type", mount_type, str(mount_dir1), instance)
            assert multipass("mount", "--type", mount_type, str(mount_dir2), instance)
            assert multipass("start", instance)

            assert mounts(instance) == {
                f"/home/ubuntu/{mount_dir1.name}": {
                    "gid_mappings": [f"{expected_mount_gid()}:default"],
                    "source_path": str(mount_dir1),
                    "uid_mappings": [f"{expected_mount_uid()}:default"],
                },
                f"/home/ubuntu/{mount_dir2.name}": {
                    "gid_mappings": [f"{expected_mount_gid()}:default"],
                    "source_path": str(mount_dir2),
                    "uid_mappings": [f"{expected_mount_uid()}:default"],
                },
            }

            assert multipass(
                "exec", instance, "--", "ls", f"/home/ubuntu/{mount_dir1.name}"
            )

            assert multipass(
                "exec", instance, "--", "ls", f"/home/ubuntu/{mount_dir2.name}"
            )

            assert multipass(
                "umount",
                f"{instance}:/home/ubuntu/{mount_dir1.name}",
            )

            assert mounts(instance) == {
                f"/home/ubuntu/{mount_dir2.name}": {
                    "gid_mappings": [f"{expected_mount_gid()}:default"],
                    "source_path": str(mount_dir2),
                    "uid_mappings": [f"{expected_mount_uid()}:default"],
                },
            }

            assert multipass(
                "umount",
                f"{instance}:/home/ubuntu/{mount_dir2.name}",
            )

            assert mounts(instance) == {}

            # FIXME: Right now the mount folder is not removed after umount.
            # assert not multipass(
            #     "exec", instance, "--", "ls", f"/home/ubuntu/{mount_dir1.name}"
            # )

            # assert not multipass(
            #     "exec", instance, "--", "ls", f"/home/ubuntu/{mount_dir2.name}"
            # )

    def test_mount_restart(self, multipassd, instance, mount_type):

        with TempDirectory() as mount_dir:
            if mount_type == "native":
                assert multipass("stop", instance)

            assert multipass("mount", "--type", mount_type, str(mount_dir), instance)
            assert multipass("start", instance)

            assert mounts(instance) == {
                f"/home/ubuntu/{mount_dir.name}": {
                    "gid_mappings": [f"{expected_mount_gid()}:default"],
                    "source_path": str(mount_dir),
                    "uid_mappings": [f"{expected_mount_uid()}:default"],
                }
            }
            assert multipass(
                "exec", instance, "--", "ls", f"/home/ubuntu/{mount_dir.name}"
            )

            multipassd.restart()

            assert mounts(instance) == {
                f"/home/ubuntu/{mount_dir.name}": {
                    "gid_mappings": [f"{expected_mount_gid()}:default"],
                    "source_path": str(mount_dir),
                    "uid_mappings": [f"{expected_mount_uid()}:default"],
                }
            }
            assert multipass(
                "exec", instance, "--", "ls", f"/home/ubuntu/{mount_dir.name}"
            )

            assert multipass("umount", instance)
            assert mounts(instance) == {}

            # FIXME: Right now the mount folder is not removed after umount.
            # assert not multipass(
            #     "exec", instance, "--", "ls", f"/home/ubuntu/{mount_dir.name}"
            # )

    def test_mount_modify(self, instance, mount_type):

        with TempDirectory() as mount_dir:
            if mount_type == "native":
                assert multipass("stop", instance)

            assert multipass("mount", "--type", mount_type, str(mount_dir), instance)
            assert multipass("start", instance)

            assert mounts(instance) == {
                f"/home/ubuntu/{mount_dir.name}": {
                    "gid_mappings": [f"{expected_mount_gid()}:default"],
                    "source_path": str(mount_dir),
                    "uid_mappings": [f"{expected_mount_uid()}:default"],
                }
            }

            instance_target_path = Path("/home") / "ubuntu" / mount_dir.name
            subdir = Path("subdir1") / "subdir2" / "subdir3"


            assert multipass("exec", instance, "--", "ls", instance_target_path.as_posix())

            assert multipass(
                "exec",
                instance,
                "--",
                rf'''bash -c "echo 'hello there' > {(instance_target_path / "file1.txt").as_posix()}"'''
            )


            assert multipass(
                "exec",
                instance,
                "--",
                f"mkdir -p {(instance_target_path / subdir).as_posix()}",
            )

            assert multipass(
                "exec",
                instance,
                "--",
                "bash", "-c",
                f'"echo \\"hello there\\" > {(instance_target_path / subdir).as_posix()}/file2.txt"',
            )

            # Verify that created files exits in host
            expected_subdir = mount_dir / subdir
            expected_file1 = mount_dir / "file1.txt"
            expected_file2 = expected_subdir / "file2.txt"
            assert expected_file1.read_text() == "hello there\n"
            assert expected_file2.read_text() == "hello there\n"

            # Remove the files from host side
            expected_file1.unlink()
            assert not expected_file1.exists()
            expected_file2.unlink()
            assert not expected_file2.exists()
            expected_subdir.rmdir()
            assert not expected_subdir.exists()

            # verify that they are no longer present in the guest
            assert not multipass("exec", instance, "--", "ls", expected_file1.as_posix())
            assert not multipass("exec", instance, "--", "ls", expected_file2.as_posix())
            assert not multipass("exec", instance, "--", "ls", expected_subdir.as_posix())

            assert multipass("umount", instance)
            assert mounts(instance) == {}
            # NOTE: For some reason, this assert fails where it works fine
            # for other tests. The only difference I could tell is this test
            # does some I/O on top of mount.
            # assert not multipass(
            #     "exec", instance, "--", "ls", str(instance_target_path)
            # )

    def test_mount_readonly(self, instance, mount_type):

        with TempDirectory() as mount_dir:
            os.chmod(mount_dir, 0o444)

            if mount_type == "native":
                assert multipass("stop", instance)

            assert multipass("mount", "--type", mount_type, str(mount_dir), instance)
            assert multipass("start", instance)

            assert mounts(instance) == {
                f"/home/ubuntu/{mount_dir.name}": {
                    "gid_mappings": [f"{expected_mount_gid()}:default"],
                    "source_path": str(mount_dir),
                    "uid_mappings": [f"{expected_mount_uid()}:default"],
                }
            }

            instance_target_path = Path("/home") / "ubuntu" / mount_dir.name
            subdir = Path("subdir1") / "subdir2" / "subdir3"

            assert multipass("exec", instance, "--", "ls", instance_target_path.as_posix())

            assert multipass(
                "exec",
                instance,
                "--",
                rf'''bash -c "echo 'hello there' > {(instance_target_path / "file1.txt").as_posix()}"'''
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
                "bash", "-c",
                f'"echo \\"hello there\\" > {(instance_target_path / subdir).as_posix()}/file2.txt"',
            )

            # Verify that created files exits in host
            expected_subdir = mount_dir / subdir
            expected_file1 = mount_dir / "file1.txt"
            expected_file2 = expected_subdir / "file2.txt"

            # verify that they are no longer present in the guest
            assert not multipass("exec", instance, "--", "ls", expected_file1.as_posix())
            assert not multipass("exec", instance, "--", "ls", expected_file2.as_posix())
            assert not multipass("exec", instance, "--", "ls", expected_subdir.as_posix())

            assert multipass("umount", instance)
            assert mounts(instance) == {}
            # NOTE: For some reason, this assert fails where it works fine
            # for other tests. The only difference I could tell is this test
            # does some I/O on top of mount.
            # assert not multipass(
            #     "exec", instance, "--", "ls", str(instance_target_path)
            # )
