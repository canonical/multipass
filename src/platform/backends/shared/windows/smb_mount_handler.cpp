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
#include <multipass/ssh/ssh_session.h>
#include <multipass/utils.h>
#include <multipass/virtual_machine.h>

#include <QHostInfo>

namespace mp = multipass;
namespace mpl = multipass::logging;
namespace mpu = multipass::utils;

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

void install_cifs_for(const std::string& name, mp::SSHSession& session, const std::chrono::milliseconds& timeout)
try
{
    mpl::log(mpl::Level::info, category, fmt::format("Installing cifs-utils in '{}'", name));

    auto proc = session.exec("sudo apt-get install -y cifs-utils");
    if (proc.exit_code(timeout) != 0)
    {
        auto error_msg = proc.read_std_error();
        mpl::log(mpl::Level::warning, category,
                 fmt::format("Failed to install 'cifs-utils', error message: '{}'", mp::utils::trim_end(error_msg)));
        throw std::runtime_error("Failed to install cifs-utils");
    }
}
catch (const mp::ExitlessSSHProcessException&)
{
    mpl::log(mpl::Level::info, category, fmt::format("Timeout while installing 'cifs-utils' in '{}'", name));
    throw std::runtime_error("Timeout installing cifs-utils");
}
} // namespace

namespace multipass
{
SmbMountHandler::SmbMountHandler(VirtualMachine* vm, const SSHKeyProvider* ssh_key_provider, const std::string& target,
                                 const VMMount& mount)
    : MountHandler{vm, ssh_key_provider, target, mount},
      source{mount.source_path},
      // share name must be unique and 80 chars max
      share_name{QString("%1_%2:%3")
                     .arg(mpu::make_uuid(), QString::fromStdString(vm->vm_name), QString::fromStdString(target))
                     .left(80)}
{
    mpl::log(mpl::Level::info, category,
             fmt::format("initializing native mount {} => {} in '{}'", mount.source_path, target, vm->vm_name));
}

void SmbMountHandler::start_impl(ServerVariant server, std::chrono::milliseconds timeout)
{
    auto source_dir_owner = get_source_dir_owner(QString::fromStdString(source));
    if (!mp::PowerShell::exec({"New-SmbShare", "-Name", share_name, "-Path", QString::fromStdString(source),
                               "-FullAccess", source_dir_owner},
                              category))
        throw std::runtime_error{fmt::format("failed creating SMB share for \"{}\"", source)};

    SSHSession session{vm->ssh_hostname(), vm->ssh_port(), vm->ssh_username(), *ssh_key_provider};

    const auto [username, password] = std::visit(
        [this, &session, timeout](auto&& server) {
            auto reply = server ? make_reply_from_server(server)
                                : throw std::runtime_error("Cannot start SMB mount without client connection");

            if (session.exec("dpkg-query --show --showformat='${db:Status-Status}' cifs-utils").read_std_output() !=
                "installed")
            {
                reply.set_reply_message("Enabling support for mounting");
                server->Write(reply);
                install_cifs_for(vm->vm_name, session, timeout);
            }

            reply.set_credentials_requested(true);
            if (!server->Write(reply))
                throw std::runtime_error("Cannot request user credentials from client. Aborting...");

            auto request = make_request_from_server(server);
            if (!server->Read(&request))
                throw std::runtime_error("Cannot get user credentials from client. Aborting...");

            return std::make_pair(request.user_credentials().username(), request.user_credentials().password());
        },
        server);

    if (password.empty())
        throw std::runtime_error("A password is required for SMB mounts.");

    const std::string credentials_path{"/root/.smb_credentials"};

    // The following mkdir in the instance will be replaced with refactored code
    auto mkdir_proc = session.exec(fmt::format("mkdir -p {}", target));
    if (mkdir_proc.exit_code() != 0)
        throw std::runtime_error(
            fmt::format("Cannot create \"{}\" in instance '{}': {}", target, vm->vm_name, mkdir_proc.read_std_error()));

    auto creds_proc = session.exec(
        fmt::format("sudo bash -c 'echo \"username={}\npassword={}\" > {}'", username, password, credentials_path));
    if (creds_proc.exit_code() != 0)
        throw std::runtime_error(
            fmt::format("Cannot create credentials file in instance: {}", creds_proc.read_std_error()));

    auto hostname = QHostInfo::localHostName();
    auto mount_proc =
        session.exec(fmt::format("sudo mount -t cifs //{}/{} {} -o credentials={},uid=$(id -u),gid=$(id -g)", hostname,
                                 share_name, target, credentials_path));
    auto mount_exit_code = mount_proc.exit_code();

    auto rm_proc = session.exec(fmt::format("sudo rm {}", credentials_path));
    if (rm_proc.exit_code() != 0)
        mpl::log(mpl::Level::warning, category,
                 fmt::format("Failed deleting credentials file in \'{}\': {}", vm->vm_name, rm_proc.read_std_error()));

    if (mount_exit_code != 0)
        throw std::runtime_error(fmt::format("Error: {}", mount_proc.read_std_error()));
}

void SmbMountHandler::remove_smb_share()
{
    if (!PowerShell::exec({"Remove-SmbShare", "-Name", share_name, "-Force"}, category))
        mpl::log(mpl::Level::warning, category,
                 fmt::format("Failed removing SMB share \"{}\" for '{}'", share_name, vm->vm_name));
}

void SmbMountHandler::stop_impl()
{
    SSHSession session{vm->ssh_hostname(), vm->ssh_port(), vm->ssh_username(), *ssh_key_provider};
    mpu::run_in_ssh_session(session, fmt::format("if mountpoint -q {0}; then sudo umount {0}; else true; fi", target));
    remove_smb_share();
}

SmbMountHandler::~SmbMountHandler()
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
        remove_smb_share();
    }
}
} // namespace multipass
