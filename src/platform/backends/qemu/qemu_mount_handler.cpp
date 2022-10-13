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

#include <multipass/exceptions/exitless_sshprocess_exception.h>
#include <multipass/file_ops.h>
#include <multipass/format.h>
#include <multipass/logging/log.h>
#include <multipass/ssh/ssh_session.h>
#include <multipass/utils.h>
#include <multipass/virtual_machine.h>
#include <multipass/vm_mount.h>

#include <QDir>
#include <QUuid>

namespace mp = multipass;
namespace mpl = multipass::logging;
namespace mpu = multipass::utils;

namespace
{
constexpr auto category = "qemu-mount-handler";
} // namespace

mp::QemuMountHandler::QemuMountHandler(const SSHKeyProvider& ssh_key_provider) : MountHandler(ssh_key_provider)
{
}

void mp::QemuMountHandler::init_mount(VirtualMachine* vm, const std::string& target_path, const VMMount& vm_mount)
{
    auto state = vm->current_state();
    if (state != mp::VirtualMachine::State::stopped && state != mp::VirtualMachine::State::off)
        throw std::runtime_error("Please shutdown virtual machine before defining native mount.");

    if (!MP_FILEOPS.exists(QDir{QString::fromStdString(vm_mount.source_path)}))
    {
        throw std::runtime_error(fmt::format("Mount path \"{}\" does not exist.", vm_mount.source_path));
    }

    // Need to ensure no more than one uid/gid map is passed in here.
    if (vm_mount.uid_mappings.size() > 1 || vm_mount.gid_mappings.size() > 1)
    {
        throw std::runtime_error("Only one mapping per native mount allowed.");
    }

    mpl::log(mpl::Level::info, category,
             fmt::format("Initializing native mount {} => {} in {}", vm_mount.source_path, target_path, vm->vm_name));
    vm->add_vm_mount(target_path, vm_mount);

    auto uid = vm_mount.uid_mappings.size() > 0 && vm_mount.uid_mappings.at(0).second != -1
                   ? vm_mount.uid_mappings.at(0).second
                   : 1000;
    mounts[vm->vm_name][target_path] = std::make_pair(vm, uid);
}

void mp::QemuMountHandler::start_mount(VirtualMachine* vm, ServerVariant server, const std::string& target_path,
                                       const std::chrono::milliseconds& timeout)
{
    SSHSession session{vm->ssh_hostname(), vm->ssh_port(), vm->ssh_username(), *ssh_key_provider};

    // Split the path in existing and missing parts
    const auto& [leading, missing] = mpu::get_path_split(session, target_path);

    auto output = mpu::run_in_ssh_session(session, "id -u");
    mpl::log(mpl::Level::debug, category,
             fmt::format("{}:{} {}(): `id -u` = {}", __FILE__, __LINE__, __FUNCTION__, output));
    auto default_uid = std::stoi(output);

    output = mpu::run_in_ssh_session(session, "id -g");
    mpl::log(mpl::Level::debug, category,
             fmt::format("{}:{} {}(): `id -g` = {}", __FILE__, __LINE__, __FUNCTION__, output));
    auto default_gid = std::stoi(output);

    // We need to create the part of the path which does not still exist, and set then the correct ownership.
    if (missing != ".")
    {
        mpu::make_target_dir(session, leading, missing);
        mpu::set_owner_for(session, leading, missing, default_uid, default_gid);
    }

    auto mount = mounts[vm->vm_name][target_path];

    // Create a reproducible unique mount tag for each mount. The cmd arg can only be 31 bytes long so part of the
    // uuid must be truncated. First character of mount_tag must also be alpabetical.
    auto mount_tag = QUuid::createUuidV3(QUuid(), QString::fromStdString(target_path))
                         .toString(QUuid::WithoutBraces)
                         .replace("-", "");
    mount_tag.truncate(30);

    mpu::run_in_ssh_session(
        session, fmt::format("sudo mount -t 9p m{} {} -o trans=virtio,version=9p2000.L,msize=536870912,access={}",
                             mount_tag, target_path, mount.second));
}

void mp::QemuMountHandler::stop_mount(const std::string& instance, const std::string& path)
{
    auto mount_it = mounts.find(instance);
    if (mount_it == mounts.end())
    {
        mpl::log(mpl::Level::info, category,
                 fmt::format("No native mount defined for \"{}\" serving '{}'", instance, path));
        return;
    }

    auto& mount_map = mount_it->second;
    auto map_entry = mount_map.find(path);
    if (map_entry != mount_map.end())
    {
        auto& vm = map_entry->second.first;
        SSHSession session{vm->ssh_hostname(), vm->ssh_port(), vm->ssh_username(), *ssh_key_provider};
        mpl::log(mpl::Level::info, category,
                 fmt::format("Stopping native mount '{}' in instance \"{}\"", path, instance));

        try
        {
            mpu::run_in_ssh_session(session, fmt::format("sudo umount {}", path));
        }
        catch (const std::exception& e)
        {
            mpl::log(mpl::Level::info, category, fmt::format("Error stopping native mount at {}", path));
        }

        vm->delete_vm_mount(path);
        mounts[instance].erase(path);
    }
}

void mp::QemuMountHandler::stop_all_mounts_for_instance(const std::string& instance)
{
    auto mounts_it = mounts.find(instance);
    if (mounts_it == mounts.end() || mounts_it->second.empty())
    {
        mpl::log(mpl::Level::info, category, fmt::format("No native mounts to stop for instance \"{}\"", instance));
    }
    else
    {
        for (auto it = mounts_it->second.cbegin(); it != mounts_it->second.cend();)
        {
            // Clever postfix increment with member access needed to prevent iterator invalidation since iterable is
            // modified in stop_mount() function
            stop_mount(instance, it++->first);
        }
    }
    mounts[instance].clear();
}

bool mp::QemuMountHandler::has_instance_already_mounted(const std::string& instance, const std::string& path) const
{
    auto entry = mounts.find(instance);
    return entry != mounts.end() && entry->second.find(path) != entry->second.end();
}
