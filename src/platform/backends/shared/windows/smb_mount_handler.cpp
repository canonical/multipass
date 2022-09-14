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

#include "smb_mount_handler.h"

#include <multipass/format.h>
#include <multipass/logging/log.h>
#include <multipass/virtual_machine.h>
#include <multipass/vm_mount.h>

namespace mp = multipass;
namespace mpl = multipass::logging;

namespace
{
constexpr auto category = "smb-mount-handler";
} // namespace

mp::SmbMountHandler::SmbMountHandler(const SSHKeyProvider& ssh_key_provider) : MountHandler(ssh_key_provider)
{
}

void mp::SmbMountHandler::init_mount(VirtualMachine* vm, const std::string& target_path, const VMMount& vm_mount)
{
    mpl::log(mpl::Level::info, category,
             fmt::format("initializing native mount {} => {} in {}", vm_mount.source_path, target_path, vm->vm_name));
    vm->add_vm_mount(target_path, vm_mount);
}

void mp::SmbMountHandler::start_mount(VirtualMachine* vm, ServerVariant server, const std::string& target_path,
                                      const std::chrono::milliseconds& timeout)
{
}

void mp::SmbMountHandler::stop_mount(const std::string& instance, const std::string& path)
{
}

void mp::SmbMountHandler::stop_all_mounts_for_instance(const std::string& instance)
{
}

bool mp::SmbMountHandler::has_instance_already_mounted(const std::string& instance, const std::string& path) const
{
    return false;
}
