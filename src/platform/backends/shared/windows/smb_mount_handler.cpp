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
#include "powershell.h"

#include <multipass/format.h>
#include <multipass/logging/log.h>
#include <multipass/virtual_machine.h>
#include <multipass/vm_mount.h>

#include <filesystem>

namespace mp = multipass;
namespace mpl = multipass::logging;

namespace
{
constexpr auto category = "smb-mount-handler";

auto get_source_dir_owner(const QString& source_dir)
{
    QString ps_output;
    QStringList get_acl{"Get-Acl", source_dir};

    mp::PowerShell::exec({QStringList() << get_acl << mp::PowerShell::Snippets::expand_property << "Owner"}, category,
                         ps_output);

    return ps_output;
}

void create_smb_share_for(const std::string& source_path, const QString& vm_name, const QString& source_dir_owner)
{
    auto share_name = QString("%1_%2").arg(vm_name).arg(
        QString::fromStdString(std::filesystem::path(source_path).filename().generic_string()));

    mp::PowerShell::exec({QStringList() << "New-SmbShare"
                                        << "-Name" << share_name << "-Path" << QString::fromStdString(source_path)
                                        << "-FullAccess" << source_dir_owner},
                         category);
}
} // namespace

mp::SmbMountHandler::SmbMountHandler(const SSHKeyProvider& ssh_key_provider) : MountHandler(ssh_key_provider)
{
}

void mp::SmbMountHandler::init_mount(VirtualMachine* vm, const std::string& target_path, const VMMount& vm_mount)
{
    mpl::log(mpl::Level::info, category,
             fmt::format("initializing native mount {} => {} in {}", vm_mount.source_path, target_path, vm->vm_name));

    auto source_dir_owner = get_source_dir_owner(QString::fromStdString(vm_mount.source_path));

    create_smb_share_for(vm_mount.source_path, QString::fromStdString(vm->vm_name), source_dir_owner);

    smb_mount_map[vm->vm_name][target_path] = vm_mount.source_path;
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
