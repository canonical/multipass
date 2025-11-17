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
from pathlib import Path
from contextlib import contextmanager

import pytest

from cli_tests.utilities import TempDirectory, retry

from cli_tests.config import cfg

from cli_tests.multipass import (
    multipass,
    mounts,
    default_mount_uid,
    default_mount_gid,
    write_file,
    create_directory,
    path_exists,
    read_file,
    move_path,
)


@contextmanager
def src_to_dst(src_path):
    class PosixStrPath(Path):
        """To ensure that VM paths are stringified as POSIX paths."""

        def __str__(self):
            # Calling as_posix() from self leads to recursion since it depends
            # on __str__
            return Path(self).as_posix()

    yield PosixStrPath("/home") / "ubuntu" / src_path.name


# todo: test custom uid/gid maps


@pytest.mark.mount
@pytest.mark.parametrize("mount_type", ["classic", "native"])
@pytest.mark.usefixtures("multipassd", "windows_privileged_mounts")
class TestMount:
    """Virtual machine mount tests."""

    def test_mount(self, instance, mount_type):
        with TempDirectory() as mount_src, src_to_dst(mount_src) as mount_dst:
            if mount_type == "native":
                assert multipass("stop", instance)

            assert multipass("mount", "--type", mount_type, str(mount_src), instance)
            assert multipass("start", instance)

            assert mounts(instance) == {
                str(mount_dst): {
                    "gid_mappings": [f"{default_mount_gid()}:default"],
                    "source_path": str(mount_src),
                    "uid_mappings": [f"{default_mount_uid()}:default"],
                }
            }

            assert path_exists(instance, mount_dst)

            # LXD cannot hot-unmount.
            if cfg.driver == "lxd":
                assert multipass("stop", instance)

            assert multipass("umount", instance)
            assert mounts(instance) == {}

    def test_mount_to_specific_target(self, instance, mount_type):
        with TempDirectory() as mount_src:
            if mount_type == "native":
                assert multipass("stop", instance)

            custom_target_path = "/tmp/target-mount-path"

            assert multipass(
                "mount",
                "--type",
                mount_type,
                str(mount_src),
                f"{instance}:{custom_target_path}",
            )
            assert multipass("start", instance)

            assert mounts(instance) == {
                custom_target_path: {
                    "gid_mappings": [f"{default_mount_gid()}:default"],
                    "source_path": str(mount_src),
                    "uid_mappings": [f"{default_mount_uid()}:default"],
                }
            }

            assert path_exists(instance, custom_target_path)

            # LXD cannot hot-unmount.
            if cfg.driver == "lxd":
                assert multipass("stop", instance)

            assert multipass("umount", instance)
            assert mounts(instance) == {}

    def test_mount_same_src_again(self, instance, mount_type):
        with TempDirectory() as mount_src:
            if mount_type == "native":
                assert multipass("stop", instance)

            custom_target_path_1 = "/tmp/target-mount-path-1"
            custom_target_path_2 = "/tmp/target-mount-path-2"
            custom_target_path_3 = "/tmp/target-mount-path-3"

            assert multipass(
                "mount",
                "--type",
                mount_type,
                str(mount_src),
                f"{instance}:{custom_target_path_1}",
            )

            assert multipass(
                "mount",
                "--type",
                mount_type,
                str(mount_src),
                f"{instance}:{custom_target_path_2}",
            )

            assert multipass(
                "mount",
                "--type",
                mount_type,
                str(mount_src),
                f"{instance}:{custom_target_path_3}",
            )

            assert multipass("start", instance)

            assert mounts(instance) == {
                custom_target_path_1: {
                    "gid_mappings": [f"{default_mount_gid()}:default"],
                    "source_path": str(mount_src),
                    "uid_mappings": [f"{default_mount_uid()}:default"],
                },
                custom_target_path_2: {
                    "gid_mappings": [f"{default_mount_gid()}:default"],
                    "source_path": str(mount_src),
                    "uid_mappings": [f"{default_mount_uid()}:default"],
                },
                custom_target_path_3: {
                    "gid_mappings": [f"{default_mount_gid()}:default"],
                    "source_path": str(mount_src),
                    "uid_mappings": [f"{default_mount_uid()}:default"],
                },
            }

            assert path_exists(instance, custom_target_path_1)
            assert path_exists(instance, custom_target_path_2)
            assert path_exists(instance, custom_target_path_3)

            # LXD cannot hot-unmount.
            if cfg.driver == "lxd":
                assert multipass("stop", instance)

            assert multipass("umount", instance)
            assert mounts(instance) == {}

    def test_mount_multiple(self, instance, mount_type):
        with (
            TempDirectory() as mount_src1,
            src_to_dst(mount_src1) as mount_dst1,
            TempDirectory() as mount_src2,
            src_to_dst(mount_src2) as mount_dst2,
        ):
            if mount_type == "native":
                assert multipass("stop", instance)

            assert multipass("mount", "--type", mount_type, str(mount_src1), instance)
            assert multipass("mount", "--type", mount_type, str(mount_src2), instance)
            assert multipass("start", instance)

            assert mounts(instance) == {
                str(mount_dst1): {
                    "gid_mappings": [f"{default_mount_gid()}:default"],
                    "source_path": str(mount_src1),
                    "uid_mappings": [f"{default_mount_uid()}:default"],
                },
                str(mount_dst2): {
                    "gid_mappings": [f"{default_mount_gid()}:default"],
                    "source_path": str(mount_src2),
                    "uid_mappings": [f"{default_mount_uid()}:default"],
                },
            }

            assert path_exists(instance, mount_dst1)
            assert path_exists(instance, mount_dst2)

            # LXD cannot hot-unmount.
            if cfg.driver == "lxd":
                assert multipass("stop", instance)

            assert multipass(
                "umount",
                f"{instance}:{str(mount_dst1)}",
            )

            assert mounts(instance) == {
                str(mount_dst2): {
                    "gid_mappings": [f"{default_mount_gid()}:default"],
                    "source_path": str(mount_src2),
                    "uid_mappings": [f"{default_mount_uid()}:default"],
                },
            }

            assert multipass(
                "umount",
                f"{instance}:{str(mount_dst2)}",
            )

            assert mounts(instance) == {}

            # FIXME: Right now the mount folder is not removed after umount.
            # assert not multipass(
            #     "exec", instance, "--", "ls", f"/home/ubuntu/{mount_dir1.name}"
            # )

            # assert not multipass(
            #     "exec", instance, "--", "ls", f"/home/ubuntu/{mount_dir2.name}"
            # )

    @pytest.mark.parametrize("restart_type", ["vm", "daemon"])
    @pytest.mark.xfail(reason="Known bug: classic mounts might not be present after a reboot", strict=False)
    def test_mount_restart(self, multipassd, instance, mount_type, restart_type):
        with TempDirectory() as mount_src, src_to_dst(mount_src) as mount_dst:
            if mount_type == "native":
                assert multipass("stop", instance)

            assert multipass("mount", "--type", mount_type, str(mount_src), instance)
            assert multipass("start", instance)

            assert mounts(instance) == {
                str(mount_dst): {
                    "gid_mappings": [f"{default_mount_gid()}:default"],
                    "source_path": str(mount_src),
                    "uid_mappings": [f"{default_mount_uid()}:default"],
                }
            }

            assert path_exists(instance, mount_dst)
            assert write_file(instance, mount_dst / "test_file.txt", "mount doom")
            assert path_exists(instance, mount_dst / "test_file.txt")
            assert (mount_src / "test_file.txt").read_text() == "mount doom\n"

            if restart_type == "daemon":
                multipassd.restart()
            elif restart_type == "vm":
                assert multipass("restart", instance)
            else:
                pytest.fail(f"Unknown restart type {restart_type}")

            # The instance should be auto-started by the daemon after restart.
            # We're doing this to just to wait until it does.

            assert multipass("start", instance)

            expected_mounts = {
                str(mount_dst): {
                    "gid_mappings": [f"{default_mount_gid()}:default"],
                    "source_path": str(mount_src),
                    "uid_mappings": [f"{default_mount_uid()}:default"],
                }
            }

            @retry(retries=10, delay=5.0)
            def mounts_check_with_retry():
                # This part is indeterministic. Rely on retry logic for now.
                return mounts(instance) == expected_mounts

            from cli_tests.multipass import debug_interactive_shell

            # We can't assert this yet.
            if not mounts_check_with_retry():
                debug_interactive_shell(instance)
                assert mounts_check_with_retry(), f"{mounts(instance)} != {expected_mounts}"
            assert mounts_check_with_retry(), f"{mounts(instance)} != {expected_mounts}"


            assert path_exists(instance, mount_dst)
            assert path_exists(instance, mount_dst / "test_file.txt")
            assert read_file(instance, mount_dst / "test_file.txt") == "mount doom"

            # LXD cannot hot-unmount.
            if cfg.driver == "lxd":
                assert multipass("stop", instance)

            assert multipass("umount", instance)

            assert mounts(instance) == {}

            # FIXME: Right now the mount folder is not removed after umount.
            # assert not multipass(
            #     "exec", instance, "--", "ls", f"/home/ubuntu/{mount_dir.name}"
            # )

    def test_mount_modify(self, instance, mount_type):
        with TempDirectory() as mount_src, src_to_dst(mount_src) as mount_dst:
            if mount_type == "native":
                assert multipass("stop", instance)

            assert multipass("mount", "--type", mount_type, str(mount_src), instance)
            assert multipass("start", instance)

            assert mounts(instance) == {
                str(mount_dst): {
                    "gid_mappings": [f"{default_mount_gid()}:default"],
                    "source_path": str(mount_src),
                    "uid_mappings": [f"{default_mount_uid()}:default"],
                }
            }

            subdir = Path("subdir1") / "subdir2" / "subdir3"

            assert path_exists(instance, mount_dst)
            assert write_file(instance, mount_dst / "file1.txt", "hello there")
            assert create_directory(instance, mount_dst / subdir)
            assert write_file(instance, mount_dst / subdir / "file2.txt", "hello there")

            # Verify that created files exits in host
            expected_subdir = mount_src / subdir
            expected_file1 = mount_src / "file1.txt"
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
            assert not path_exists(instance, expected_file1)
            assert not path_exists(instance, expected_file2)
            assert not path_exists(instance, expected_subdir)

            # LXD cannot hot-unmount.
            if cfg.driver == "lxd":
                assert multipass("stop", instance)

            assert multipass("umount", instance)
            assert mounts(instance) == {}
            # NOTE: For some reason, this assert fails where it works fine
            # for other tests. The only difference I could tell is this test
            # does some I/O on top of mount.
            # assert not multipass(
            #     "exec", instance, "--", "ls", str(instance_target_path)
            # )

    def test_mount_rename(self, instance, mount_type):
        with TempDirectory() as mount_src, src_to_dst(mount_src) as mount_dst:
            if mount_type == "native":
                assert multipass("stop", instance)

            assert multipass("mount", "--type", mount_type, str(mount_src), instance)
            assert multipass("start", instance)

            assert mounts(instance) == {
                str(mount_dst): {
                    "gid_mappings": [f"{default_mount_gid()}:default"],
                    "source_path": str(mount_src),
                    "uid_mappings": [f"{default_mount_uid()}:default"],
                }
            }

            subdir = Path("subdir1") / "subdir2" / "subdir3"

            assert path_exists(instance, mount_dst)
            assert write_file(instance, mount_dst / "file1.txt", "hello there")
            assert create_directory(instance, mount_dst / subdir)
            assert write_file(instance, mount_dst / subdir / "file2.txt", "hello there")

            assert move_path(
                instance,
                mount_dst / subdir / "file2.txt",
                mount_dst / subdir / "file2_moved.txt",
            )

            assert not path_exists(instance, mount_dst / subdir / "file2.txt")
            assert path_exists(instance, mount_dst / subdir / "file2_moved.txt")

            assert move_path(
                instance, mount_dst / "subdir1", mount_dst / "subdir1_moved"
            )

            assert not path_exists(instance, mount_dst / subdir)
            assert not path_exists(instance, mount_dst / subdir / "file2.txt")
            assert path_exists(instance, mount_dst / "file1.txt")
            assert path_exists(instance, mount_dst / mount_dst / "subdir1_moved")
            assert path_exists(
                instance,
                mount_dst
                / mount_dst
                / "subdir1_moved"
                / "subdir2"
                / "subdir3"
                / "file2_moved.txt",
            )

            # LXD cannot hot-unmount.
            if cfg.driver == "lxd":
                assert multipass("stop", instance)

            assert multipass("umount", instance)
            assert mounts(instance) == {}
            # NOTE: For some reason, this assert fails where it works fine
            # for other tests. The only difference I could tell is this test
            # does some I/O on top of mount.
            # assert not multipass(
            #     "exec", instance, "--", "ls", str(instance_target_path)
            # )

    def test_mount_readonly(self, instance, mount_type):
        with TempDirectory() as mount_src, src_to_dst(mount_src) as mount_dst:
            os.chmod(mount_src, 0o444)

            if mount_type == "native":
                assert multipass("stop", instance)

            assert multipass("mount", "--type", mount_type, str(mount_src), instance)
            assert multipass("start", instance)

            assert mounts(instance) == {
                str(mount_dst): {
                    "gid_mappings": [f"{default_mount_gid()}:default"],
                    "source_path": str(mount_src),
                    "uid_mappings": [f"{default_mount_uid()}:default"],
                }
            }

            subdir = Path("subdir1") / "subdir2" / "subdir3"

            # Native mounts shouldn't allow write to read-only targets.
            # At least it's the case with the QEMU native mounts.
            cannot_write = mount_type == "native"

            assert path_exists(instance, mount_dst)

            assert (
                write_file(instance, mount_dst / "file1.txt", "hello there")
                or cannot_write
            )

            assert create_directory(instance, mount_dst / subdir) or cannot_write

            assert (
                write_file(instance, mount_dst / subdir / "file2.txt", "hello there")
                or cannot_write
            )

            # verify that they are present in the guest
            assert path_exists(instance, mount_dst / "file1.txt") or cannot_write
            assert path_exists(instance, mount_dst / subdir) or cannot_write
            assert (
                path_exists(instance, mount_dst / subdir / "file2.txt") or cannot_write
            )

            # LXD cannot hot-unmount.
            if cfg.driver == "lxd":
                assert multipass("stop", instance)

            assert multipass("umount", instance)
            assert mounts(instance) == {}
            # NOTE: For some reason, this assert fails where it works fine
            # for other tests. The only difference I could tell is this test
            # does some I/O on top of mount.
            # assert not multipass(
            #     "exec", instance, "--", "ls", str(instance_target_path)
            # )
