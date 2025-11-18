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

from .basics import (
    get_multipass_env,
    get_multipass_path,
    get_multipassd_path,
    determine_storage_dir,
    determine_data_dir,
    determine_bin_dir,
)
from .exceptions import TestSessionFailure, TestCaseFailure
from .certutils import authenticate_client_cert, get_client_cert_path
from .helpers import (
    debug_interactive_shell,
    exec,
    path_exists,
    read_file,
    write_file,
    move_path,
    create_directory,
    get_core_count,
    get_disk_size,
    get_ram_size,
    mounts,
    shell,
    snapshot_count,
    state,
    info,
    default_driver_name,
    default_daemon_controller,
    default_mount_gid,
    default_mount_uid,
    get_default_interface_name,
    get_mac_addr_of,
    get_cloudinit_instance_id,
    get_multipass_version,
    vm_exists,
)
from .multipass_cmd import multipass
from .snapshot import build_snapshot_tree, collapse_to_snapshot_tree, take_snapshot
from .validate import (
    validate_info_output,
    validate_list_output,
    validate_info_snapshot_output,
)
from .cmd_output import Output
from .imageutils import image_name_to_version
from .launch import launch
from .fileutils import nuke_all_instances
from .nameutils import random_vm_name
from .feature_versions import multipass_version_has_feature, skip_if_feature_not_supported
