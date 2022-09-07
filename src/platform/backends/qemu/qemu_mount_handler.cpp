/*
 * Copyright (C) 2022 Canonical, Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "qemu_mount_handler.h"

#include <multipass/format.h>
#include <multipass/logging/log.h>
#include <multipass/virtual_machine.h>
#include <multipass/vm_mount.h>

namespace mp = multipass;
namespace mpl = multipass::logging;

namespace
{
constexpr auto category = "qemu-mount-handler";
} // namespace

mp::QemuMountHandler::QemuMountHandler(const SSHKeyProvider& ssh_key_provider) : MountHandler(ssh_key_provider)
{
}

void mp::QemuMountHandler::init_mount(VirtualMachine* vm, const std::string& target_path, const VMMount& vm_mount)
{
    // Need to ensure no more than one uid/gid map is passed in here.

    mpl::log(mpl::Level::info, category,
             fmt::format("initializing native mount {} => {} in {}", vm_mount.source_path, target_path, vm->vm_name));
    vm->add_vm_mount(target_path, vm_mount);

    // Need to save the vm pointer so it can be used later in stop.
}

void mp::QemuMountHandler::start_mount(VirtualMachine* vm, ServerVariant server, const std::string& target_path,
                                       const std::chrono::milliseconds& timeout)
{
    // Need to SSHSession::exec() into the instance and run something like:
    // sudo mount -t 9p ${mount_tag} ${target_path} -o trans=virtio,version=9p2000.L,msize=536870912
    // where:
    // ${mount_tag} is the same mount_tag used in add_vm_mount()
    // ${target_path} is target_path
    // We may need to play around with the options here such as msize or other necessary options.
}

void mp::QemuMountHandler::stop_mount(const std::string& instance, const std::string& path)
{
    // The needs to SSHSession::exec() into the instance and unmount the path
    // vm->delete_vm_mount(path);
}

void mp::QemuMountHandler::stop_all_mounts_for_instance(const std::string& instance)
{
    // This just needs to iterate over all mounts for an instance and stop them
}

bool mp::QemuMountHandler::has_instance_already_mounted(const std::string& instance, const std::string& path) const
{
    // Just returns if a mount is already running for the given instance and target_path
    return false;
}
