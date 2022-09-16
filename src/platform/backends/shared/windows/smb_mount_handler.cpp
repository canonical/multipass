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

#include <multipass/exceptions/exitless_sshprocess_exception.h>
#include <multipass/format.h>
#include <multipass/logging/log.h>
#include <multipass/ssh/ssh_session.h>
#include <multipass/utils.h>
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

auto create_smb_share_for(const std::string& source_path, const QString& vm_name, const QString& source_dir_owner)
{
    auto share_name = QString("%1_%2").arg(vm_name).arg(
        QString::fromStdString(std::filesystem::path(source_path).filename().generic_string()));

    mp::PowerShell::exec({QStringList() << "New-SmbShare"
                                        << "-Name" << share_name << "-Path" << QString::fromStdString(source_path)
                                        << "-FullAccess" << source_dir_owner},
                         category);

    return share_name;
}

void install_cifs_for(const std::string& name, mp::SSHSession& session, std::function<void()> const& on_install,
                      const std::chrono::milliseconds& timeout)
{
    if (session.exec("dpkg -l | grep cifs-utils").exit_code() == 0)
    {
        return;
    }

    try
    {
        mpl::log(mpl::Level::info, category, fmt::format("Installing cifs-utils in \'{}\'", name));

        on_install();

        auto proc = session.exec("sudo apt-get install -y cifs-utils");
        if (proc.exit_code(timeout) != 0)
        {
            auto error_msg = proc.read_std_error();
            mpl::log(
                mpl::Level::warning, category,
                fmt::format("Failed to install \'cifs-utils\', error message: \'{}\'", mp::utils::trim_end(error_msg)));
            throw std::runtime_error("Failed to install cifs-utils");
        }
    }
    catch (const mp::ExitlessSSHProcessException&)
    {
        mpl::log(mpl::Level::info, category, fmt::format("Timeout while installing 'cifs-utils' in '{}'", name));
        throw std::runtime_error("Timeout installing cifs-utils");
    }
}

auto get_gateway_ip_address()
{
    QString ps_output;

    mp::PowerShell::exec({QStringList() << "Get-NetIPAddress"
                                        << "-InterfaceAlias"
                                        << "'vEthernet (Default Switch)'"
                                        << "-AddressFamily"
                                        << "IPv4" << mp::PowerShell::Snippets::expand_property << "IPAddress"},
                         category, ps_output);

    return ps_output;
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

    auto share_name = create_smb_share_for(vm_mount.source_path, QString::fromStdString(vm->vm_name), source_dir_owner);

    smb_mount_map[vm->vm_name][target_path] = share_name;
}

void mp::SmbMountHandler::start_mount(VirtualMachine* vm, ServerVariant server, const std::string& target_path,
                                      const std::chrono::milliseconds& timeout)
{
    SSHSession session{vm->ssh_hostname(), vm->ssh_port(), vm->ssh_username(), *ssh_key_provider};

    std::visit(
        [this, vm, &session, &timeout](auto&& server) {
            auto on_install = [this, server] {
                if (server)
                {
                    auto reply = make_reply_from_server(*server);
                    reply.set_reply_message("Enabling support for mounting");
                    server->Write(reply);
                }
            };

            install_cifs_for(vm->vm_name, session, on_install, timeout);
        },
        server);

    auto gateway_ip = get_gateway_ip_address();
    auto share_name = smb_mount_map[vm->vm_name][target_path];

    auto proc = session.exec(
        fmt::format("sudo mount -t cifs //{}/{} {} -o credentials=/root/.smb_credentials,uid=$(id -u),gid=$(id -g)",
                    gateway_ip, share_name, target_path));

    if (proc.exit_code() != 0)
    {
        throw std::runtime_error(fmt::format("Error: {}", proc.read_std_error()));
    }
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
