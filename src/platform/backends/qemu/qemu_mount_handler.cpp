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

#include <multipass/utils.h>

#include <QUuid>

namespace mpl = multipass::logging;
namespace mpu = multipass::utils;

namespace
{
constexpr auto category = "qemu-mount-handler";
} // namespace

namespace multipass
{
QemuMountHandler::QemuMountHandler(QemuVirtualMachine* vm, const SSHKeyProvider* ssh_key_provider,
                                   const std::string& target, const VMMount& mount)
    : MountHandler{vm, ssh_key_provider, target, mount},
      vm_mount_args{vm->modifiable_mount_args()},
      // Create a reproducible unique mount tag for each mount. The cmd arg can only be 31 bytes long so part of the
      // uuid must be truncated. First character of tag must also be alpabetical.
      tag{mp::utils::make_uuid().remove("-").left(30).prepend('m').toStdString()}
{
    using St = VirtualMachine::State;
    const auto skip_states = {St::off, St::stopped, St::suspended};
    if (std::find(skip_states.begin(), skip_states.end(), vm->current_state()) == skip_states.end())
        throw std::runtime_error("Please shutdown the instance before attempting native mounts.");

    // Need to ensure no more than one uid/gid map is passed in here.
    if (mount.uid_mappings.size() > 1 || mount.gid_mappings.size() > 1)
        throw std::runtime_error("Only one mapping per native mount allowed.");

    mpl::log(mpl::Level::info, category,
             fmt::format("initializing native mount {} => {} in '{}'", mount.source_path, target, vm->vm_name));

    const auto uid_map = mount.uid_mappings.empty() ? std::make_pair(1000, 1000) : mount.uid_mappings[0];
    const auto gid_map = mount.gid_mappings.empty() ? std::make_pair(1000, 1000) : mount.gid_mappings[0];
    const auto uid_arg = QString("uid_map=%1:%2,").arg(uid_map.first).arg(uid_map.second == -1 ? 1000 : uid_map.second);
    const auto gid_arg = QString{"gid_map=%1:%2,"}.arg(gid_map.first).arg(gid_map.second == -1 ? 1000 : gid_map.second);
    vm_mount_args[tag] = {
        mount.source_path,
        {"-virtfs", QString::fromStdString(fmt::format("local,security_model=passthrough,{}{}path={},mount_tag={}",
                                                       uid_arg, gid_arg, mount.source_path, tag))}};
}

void QemuMountHandler::start_impl(ServerVariant, std::chrono::milliseconds)
{
    SSHSession session{vm->ssh_hostname(), vm->ssh_port(), vm->ssh_username(), *ssh_key_provider};

    // Split the path in existing and missing parts
    const auto& [leading, missing] = mpu::get_path_split(session, target);
    const auto default_uid = std::stoi(mpu::run_in_ssh_session(session, "id -u"));
    mpl::log(mpl::Level::debug, category,
             fmt::format("{}:{} {}(): `id -u` = {}", __FILE__, __LINE__, __FUNCTION__, default_uid));
    const auto default_gid = std::stoi(mpu::run_in_ssh_session(session, "id -g"));
    mpl::log(mpl::Level::debug, category,
             fmt::format("{}:{} {}(): `id -g` = {}", __FILE__, __LINE__, __FUNCTION__, default_gid));

    // We need to create the part of the path which does not still exist, and set then the correct ownership.
    if (missing != ".")
    {
        mpu::make_target_dir(session, leading, missing);
        mpu::set_owner_for(session, leading, missing, default_uid, default_gid);
    }

    mpu::run_in_ssh_session(
        session, fmt::format("sudo mount -t 9p {} {} -o trans=virtio,version=9p2000.L,msize=536870912", tag, target));
}

void QemuMountHandler::stop_impl()
{
    mpl::log(mpl::Level::info, category,
             fmt::format("Stopping native mount \"{}\" in instance '{}'", target, vm->vm_name));
    SSHSession session{vm->ssh_hostname(), vm->ssh_port(), vm->ssh_username(), *ssh_key_provider};
    mpu::run_in_ssh_session(session, fmt::format("if mountpoint -q {0}; then sudo umount {0}; else true; fi", target));
}

QemuMountHandler::~QemuMountHandler()
{
    try
    {
        stop();
    }
    catch (const std::exception& e)
    {
        mpl::log(
            mpl::Level::warning, category,
            fmt::format("Failed to gracefully stop mount \"{}\" in instance '{}': {}", target, vm->vm_name, e.what()));
    }
    vm_mount_args.erase(tag);
}
} // namespace multipass
