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
QemuMountHandler::QemuMountHandler(VirtualMachine* vm, const SSHKeyProvider* ssh_key_provider, std::string target,
                                   const VMMount& mount)
    : MountHandler{vm, ssh_key_provider, target, mount}
{
    if (!(qemu_vm = dynamic_cast<QemuVirtualMachine*>(vm)))
        throw std::runtime_error{"QEMU mount must be used on a QEMU VM"};

    switch (vm->current_state())
    {
    case VirtualMachine::State::off:
    case VirtualMachine::State::stopped:
    case VirtualMachine::State::suspended:
        break;
    default:
        throw std::runtime_error("Please shutdown virtual machine before defining native mount.");
    }

    // Need to ensure no more than one uid/gid map is passed in here.
    if (mount.uid_mappings.size() > 1 || mount.gid_mappings.size() > 1)
        throw std::runtime_error("Only one mapping per native mount allowed.");

    mpl::log(mpl::Level::info, category,
             fmt::format("initializing native mount {} => {} in '{}'", mount.source_path, target, vm->vm_name));

    // Create a reproducible unique mount tag for each mount. The cmd arg can only be 31 bytes long so part of the
    // uuid must be truncated. First character of tag must also be alpabetical.
    tag = QUuid::createUuidV3(QUuid(), QString::fromStdString(target))
              .toString(QUuid::WithoutBraces)
              .remove("-")
              .left(30);

    const auto uid_map = mount.uid_mappings.empty() ? std::make_pair(1000, 1000) : mount.uid_mappings[0];
    const auto gid_map = mount.gid_mappings.empty() ? std::make_pair(1000, 1000) : mount.gid_mappings[0];
    const auto uid_arg = QString("uid_map=%1:%2,").arg(uid_map.first).arg(uid_map.second == -1 ? 1000 : uid_map.second);
    const auto gid_arg = QString{"gid_map=%1:%2,"}.arg(gid_map.first).arg(gid_map.second == -1 ? 1000 : gid_map.second);
    qemu_vm->mount_args[target] = {
        mount.source_path,
        {"-virtfs", QString("local,security_model=passthrough,%1%2path=%3,mount_tag=m%4")
                        .arg(uid_arg, gid_arg, QString::fromStdString(mount.source_path), tag)}};
}

void QemuMountHandler::start(ServerVariant server, std::chrono::milliseconds timeout)
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
        session, fmt::format("sudo mount -t 9p m{} {} -o trans=virtio,version=9p2000.L,msize=536870912", tag, target));
}

void QemuMountHandler::stop()
{
    if (qemu_vm->mount_args.find(target) == qemu_vm->mount_args.end())
        return;

    mpl::log(mpl::Level::info, category,
             fmt::format("Stopping native mount \"{}\" in instance '{}'", target, vm->vm_name));
    SSHSession session{vm->ssh_hostname(), vm->ssh_port(), vm->ssh_username(), *ssh_key_provider};
    mpu::run_in_ssh_session(session, fmt::format("if mountpoint -q {0}; then sudo umount {0}; else true; fi", target));
    qemu_vm->mount_args.erase(target);
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
        qemu_vm->mount_args.erase(target);
    }
}
} // namespace multipass
