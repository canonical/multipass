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

#include <QHostInfo>

namespace mp = multipass;
namespace mpl = multipass::logging;

namespace
{
constexpr auto category = "smb-mount-handler";

auto get_source_dir_owner(const QString& source_dir)
{
    QString ps_output;
    QStringList get_acl{"Get-Acl", source_dir};

    mp::PowerShell::exec(get_acl << mp::PowerShell::Snippets::expand_property << "Owner", category, ps_output);

    return ps_output;
}

auto create_smb_share_for(const std::string& source_path, const QString& vm_name, const QString& source_dir_owner)
{
    auto share_name = QString("%1_%2").arg(
        vm_name, QString::fromStdString(std::filesystem::path(source_path).filename().generic_string()));

    mp::PowerShell::exec({"New-SmbShare", "-Name", share_name, "-Path", QString::fromStdString(source_path),
                          "-FullAccess", source_dir_owner},
                         category);

    return share_name;
}

auto remove_smb_share_for(const QString& share_name)
{
    return mp::PowerShell::exec({"Remove-SmbShare", "-Name", share_name, "-Force"}, category);
}

void install_cifs_for(const std::string& name, mp::SSHSession& session, const std::chrono::milliseconds& timeout)
{
    try
    {
        mpl::log(mpl::Level::info, category, fmt::format("Installing cifs-utils in \'{}\'", name));

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

    smb_mount_map[vm->vm_name][target_path] = std::make_pair(share_name, vm);
}

void mp::SmbMountHandler::start_mount(VirtualMachine* vm, ServerVariant server, const std::string& target_path,
                                      const std::chrono::milliseconds& timeout)
{
    SSHSession session{vm->ssh_hostname(), vm->ssh_port(), vm->ssh_username(), *ssh_key_provider};

    const auto [username, password] = std::visit(
        [this, vm, &session, &timeout](auto&& server) {
            if (!server)
            {
                throw std::runtime_error("Cannot start Windows mount without client connection");
            }

            auto reply = make_reply_from_server(*server);

            if (session.exec("dpkg -l | grep cifs-utils").exit_code() != 0)
            {
                reply.set_reply_message("Enabling support for mounting");
                server->Write(reply);

                install_cifs_for(vm->vm_name, session, timeout);
            }

            reply.set_credentials_requested(true);
            if (!server->Write(reply))
            {
                throw std::runtime_error("Cannot request user credentials from client. Aborting...");
            }

            auto request = make_request_from_server(*server);
            if (!server->Read(&request))
            {
                throw std::runtime_error("Cannot get user credentials from client. Aborting...");
            }

            return std::make_pair(request.user_credentials().username(), request.user_credentials().password());
        },
        server);

    if (password.empty())
    {
        throw std::runtime_error("A password is required for Windows mounts.");
    }

    const std::string credentials_path{"/root/.smb_credentials"};

    // The following mkdir in the instance will be replaced with refactored code
    auto mkdir_proc = session.exec(fmt::format("mkdir -p {}", target_path));
    if (mkdir_proc.exit_code() != 0)
    {
        throw std::runtime_error(fmt::format("Cannot create {} in instance '{}': {}", target_path, vm->vm_name,
                                             mkdir_proc.read_std_error()));
    }

    auto creds_proc = session.exec(
        fmt::format("sudo bash -c 'echo \"username={}\npassword={}\" > {}'", username, password, credentials_path));

    if (creds_proc.exit_code() != 0)
    {
        throw std::runtime_error(
            fmt::format("Cannot create credentials file in instance: {}", creds_proc.read_std_error()));
    }

    auto hostname = QHostInfo::localHostName();
    const auto [share_name, _] = smb_mount_map[vm->vm_name][target_path];

    auto mount_proc =
        session.exec(fmt::format("sudo mount -t cifs //{}/{} {} -o credentials={},uid=$(id -u),gid=$(id -g)", hostname,
                                 share_name, target_path, credentials_path));

    auto mount_exit_code = mount_proc.exit_code();

    auto rm_proc = session.exec(fmt::format("sudo rm {}", credentials_path));
    if (rm_proc.exit_code() != 0)
    {
        mpl::log(mpl::Level::warning, category,
                 fmt::format("Failed deleting credentials file in \'{}\': {}", vm->vm_name, rm_proc.read_std_error()));
    }

    if (mount_exit_code != 0)
    {
        throw std::runtime_error(fmt::format("Error: {}", mount_proc.read_std_error()));
    }
}

void mp::SmbMountHandler::stop_mount(const std::string& instance, const std::string& path)
{
    const auto [share_name, vm] = smb_mount_map[instance][path];

    SSHSession session{vm->ssh_hostname(), vm->ssh_port(), vm->ssh_username(), *ssh_key_provider};

    auto umount_proc = session.exec(fmt::format("sudo umount {}", path));
    if (umount_proc.exit_code() != 0)
    {
        throw std::runtime_error(fmt::format("Cannot unmount share in instance: {}", umount_proc.read_std_error()));
    }

    if (!remove_smb_share_for(share_name))
    {
        mpl::log(mpl::Level::warning, category,
                 fmt::format("Failed removing share \"{}\" for \"{}\"", share_name, instance));
    }

    smb_mount_map[instance].erase(path);
}

void mp::SmbMountHandler::stop_all_mounts_for_instance(const std::string& instance) // clang-format off
try // clang-format on
{
    const auto& mount_info = smb_mount_map.at(instance);

    if (mount_info.empty())
    {
        throw std::out_of_range("");
    }

    for (auto it = mount_info.cbegin(); it != mount_info.cend();)
    {
        stop_mount(instance, it++->first);
    }
}
catch (const std::out_of_range&)
{
    mpl::log(mpl::Level::info, category, fmt::format("No mounts to stop for instance \"{}\"", instance));
}

bool mp::SmbMountHandler::has_instance_already_mounted(const std::string& instance, const std::string& path) const
{
    auto entry = smb_mount_map.find(instance);

    return entry != smb_mount_map.end() && entry->second.find(path) != entry->second.end();
}
