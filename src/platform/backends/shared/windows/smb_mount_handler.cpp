/*
 * Copyright (C) Canonical, Ltd.
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
#include "aes.h"

#include "powershell.h"

#include <multipass/exceptions/exitless_sshprocess_exception.h>
#include <multipass/file_ops.h>
#include <multipass/platform.h>
#include <multipass/ssh/sftp_utils.h>
#include <multipass/ssh/ssh_session.h>
#include <multipass/utils.h>
#include <multipass/virtual_machine.h>

#include <QHostInfo>

#include <lm.h>
#pragma comment(lib, "Netapi32.lib")

namespace mp = multipass;
namespace mpl = multipass::logging;
namespace mpu = multipass::utils;

namespace
{
constexpr auto category = "smb-mount-handler";

void install_cifs_for(const std::string& name, mp::SSHSession& session, const std::chrono::milliseconds& timeout)
try
{
    mpl::log(mpl::Level::info, category, fmt::format("Installing cifs-utils in '{}'", name));

    auto proc = session.exec("sudo apt-get update && sudo apt-get install -y cifs-utils");
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

auto quote(const QString& string)
{
    return "'" + string + "'";
}

} // namespace

namespace multipass
{
bool SmbMountHandler::smb_share_exists()
{
    PSHARE_INFO_0 share_info;

    auto res = NetShareGetInfo(nullptr, reinterpret_cast<LPWSTR>(share_name.data()), 0, (LPBYTE*)&share_info);

    NetApiBufferFree(share_info);

    return res == 0;
}

void SmbMountHandler::create_smb_share(const QString& user)
{
    if (smb_share_exists())
        return;

    if (!can_user_access_source(user))
        throw std::runtime_error{fmt::format("cannot access \"{}\"", source)};

    DWORD parm_err = 0;
    SHARE_INFO_2 share_info;
    share_info.shi2_netname = reinterpret_cast<LPWSTR>(share_name.data());
    share_info.shi2_remark = L"Multipass mount share";
    share_info.shi2_type = STYPE_DISKTREE;
    share_info.shi2_permissions = 0;
    share_info.shi2_max_uses = -1;
    share_info.shi2_current_uses = 0;
    share_info.shi2_path = reinterpret_cast<LPWSTR>(source.data());
    share_info.shi2_passwd = nullptr;

    if (NetShareAdd(nullptr, 2, (LPBYTE)&share_info, &parm_err) != 0)
        throw std::runtime_error{fmt::format("failed creating SMB share for \"{}\": {}", source, parm_err)};
}

void SmbMountHandler::remove_smb_share()
{
    if (smb_share_exists() && NetShareDel(nullptr, reinterpret_cast<LPWSTR>(share_name.data()), 0) != 0)
        mpl::log(mpl::Level::warning, category,
                 fmt::format("Failed removing SMB share \"{}\" for '{}'", share_name, vm->vm_name));
}

void SmbMountHandler::remove_cred_files(const QString& user_id)
{
    auto iv_filename = user_id + ".iv";
    auto cred_filename = user_id + ".cifs";

    QFile cred_file{cred_dir.filePath(cred_filename)};
    QFile iv_file{cred_dir.filePath(iv_filename)};

    MP_FILEOPS.remove(cred_file);
    MP_FILEOPS.remove(iv_file);
}

bool SmbMountHandler::can_user_access_source(const QString& user)
{
    // TODO: I tried to use the proper Windows API to get ACL permissions for the user being passed in, but
    // alas, the API is very convoluted. At some point, another attempt should be made to use the proper
    // API though...
    QString output;
    return PowerShell::exec(
               {QString{"(Get-Acl '%1').Access | ?{($_.IdentityReference -match '%2') -and ($_.FileSystemRights "
                        "-eq 'FullControl')}"}
                    .arg(source, user)},
               "Get ACLs", &output) &&
           !output.isEmpty();
}

void SmbMountHandler::encrypt_credentials_to_file(const QString& cred_filename, const QString& iv_filename,
                                                  const std::string& ptext)
try
{
    std::string ctext;
    QFile cred_file{cred_dir.filePath(cred_filename)};
    QFile iv_file{cred_dir.filePath(iv_filename)};
    auto iv = MP_UTILS.random_bytes(MP_AES.aes_256_block_size());

    MP_AES.encrypt(enc_key, iv, ptext, ctext);

    MP_FILEOPS.open(iv_file, QIODevice::WriteOnly);
    MP_FILEOPS.write(iv_file, (const char*)(iv.data()), MP_AES.aes_256_block_size());

    MP_FILEOPS.open(cred_file, QIODevice::WriteOnly);
    MP_FILEOPS.write(cred_file, (const char*)(ctext.data()), ctext.size());

    mpl::log(mpl::Level::info, category, "Successfully encrypted credentials");
}
catch (const std::exception& e)
{
    mpl::log(mpl::Level::warning, category, fmt::format("Failed to encrypt credentials to file: {}", e.what()));
}

std::string SmbMountHandler::decrypt_credentials_from_file(const QString& cred_filename, const QString& iv_filename)
try
{
    std::vector<uint8_t> iv;
    std::string ctext;
    std::string rtext;
    QFile cred_file{cred_dir.filePath(cred_filename)};
    QFile iv_file{cred_dir.filePath(iv_filename)};

    if (!MP_FILEOPS.exists(cred_file))
        throw std::runtime_error(fmt::format("File not found: {}", cred_filename));

    if (!MP_FILEOPS.exists(iv_file))
        throw std::runtime_error(fmt::format("File not found: {}", iv_filename));

    iv.resize(MP_AES.aes_256_block_size());
    ctext.resize(cred_file.size());

    MP_FILEOPS.open(iv_file, QIODevice::ReadOnly);
    MP_FILEOPS.read(iv_file, (char*)(iv.data()), MP_AES.aes_256_block_size());

    MP_FILEOPS.open(cred_file, QIODevice::ReadOnly);
    MP_FILEOPS.read(cred_file, ctext.data(), ctext.size());

    MP_AES.decrypt(enc_key, iv, ctext, rtext);

    mpl::log(mpl::Level::info, category, "Successfully decrypted credentials");

    return rtext;
}
catch (const std::exception& e)
{
    mpl::log(mpl::Level::warning, category, fmt::format("Failed to decrypt credentials from file: {}", e.what()));
    return {};
}

SmbMountHandler::SmbMountHandler(VirtualMachine* vm, const SSHKeyProvider* ssh_key_provider, const std::string& target,
                                 const VMMount& mount, const mp::Path& cred_dir)
    : MountHandler{vm, ssh_key_provider, target, mount.source_path},
      source{QString::fromStdString(mount.source_path)},
      // share name must be unique and 80 chars max
      share_name{QString("%1_%2:%3")
                     .arg(mpu::make_uuid(), QString::fromStdString(vm->vm_name), QString::fromStdString(target))
                     .left(80)},
      cred_dir{cred_dir}
{
    mpl::log(mpl::Level::info, category,
             fmt::format("Initializing native mount {} => {} in '{}'", mount.source_path, target, vm->vm_name));

    auto data_location{MP_PLATFORM.multipass_storage_location() + "\\data"};
    auto enc_key_dir_path{MP_UTILS.make_dir(data_location, "enc-keys",
                                            QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner)};
    auto key_file = QFile{QDir{enc_key_dir_path}.filePath("aes.key")};

    if (MP_FILEOPS.exists(key_file))
    {
        enc_key.resize(MP_AES.aes_256_key_size());
        MP_FILEOPS.open(key_file, QIODevice::ReadOnly);
        MP_FILEOPS.read(key_file, (char*)(enc_key.data()), MP_AES.aes_256_key_size());
    }
    else
    {
        enc_key = MP_UTILS.random_bytes(MP_AES.aes_256_key_size());
        MP_FILEOPS.open(key_file, QIODevice::WriteOnly);
        MP_FILEOPS.write(key_file, (const char*)(enc_key.data()), MP_AES.aes_256_key_size());
        mpl::log(mpl::Level::info, category, "Successfully generated new encryption key");
    }
}

bool SmbMountHandler::is_active()
try
{
    return active && smb_share_exists() &&
           !SSHSession{vm->ssh_hostname(), vm->ssh_port(), vm->ssh_username(), *ssh_key_provider}
                .exec(fmt::format("findmnt --type cifs | grep '{} //{}/{}'", target, QHostInfo::localHostName(),
                                  share_name))
                .exit_code();
}
catch (const std::exception& e)
{
    mpl::log(mpl::Level::warning, category,
             fmt::format("Failed checking SSHFS mount \"{}\" in instance '{}': {}", target, vm->vm_name, e.what()));
    return false;
}

void SmbMountHandler::activate_impl(ServerVariant server, std::chrono::milliseconds timeout)
try
{
    SSHSession session{vm->ssh_hostname(), vm->ssh_port(), vm->ssh_username(), *ssh_key_provider};

    const auto username = MP_PLATFORM.get_username();
    const auto user_id = mpu::make_uuid(username.toStdString());
    const auto iv_filename = user_id + ".iv";
    const auto cred_filename = user_id + ".cifs";

    if (session.exec("dpkg-query --show --showformat='${db:Status-Status}' cifs-utils").read_std_output() !=
        "installed")
    {
        auto visitor = [](auto server) {
            if (server)
            {
                auto reply = make_reply_from_server(server);
                reply.set_reply_message("Enabling support for mounting");
                server->Write(reply);
            }
        };

        std::visit(visitor, server);
        install_cifs_for(vm->vm_name, session, timeout);
    }

    const auto rtext = decrypt_credentials_from_file(cred_filename, iv_filename);
    const auto tokens = mp::utils::split(rtext, "=");
    auto password = tokens.size() == 2 ? tokens.at(1) : "";

    if (password.empty())
    {
        password = std::visit(
            [this, &session, timeout](auto&& server) {
                auto reply = server ? make_reply_from_server(server)
                                    : throw std::runtime_error("Cannot get password without client connection");

                reply.set_password_requested(true);
                if (!server->Write(reply))
                    throw std::runtime_error("Cannot request password from client. Aborting...");

                auto request = make_request_from_server(server);
                if (!server->Read(&request))
                    throw std::runtime_error("Cannot get password from client. Aborting...");

                return request.password();
            },
            server);

        if (password.empty())
            throw std::runtime_error("A password is required for SMB mounts.");

        encrypt_credentials_to_file(cred_filename, iv_filename, fmt::format("password={}", password));
    }

    create_smb_share(username);

    // The following mkdir in the instance will be replaced with refactored code
    auto mkdir_proc = session.exec(fmt::format("mkdir -p {}", target));
    if (mkdir_proc.exit_code() != 0)
        throw std::runtime_error(
            fmt::format("Cannot create \"{}\" in instance '{}': {}", target, vm->vm_name, mkdir_proc.read_std_error()));

    auto smb_creds = fmt::format("username={}\npassword={}", username, password);
    const std::string credentials_path{"/tmp/.smb_credentials"};
    auto sftp_client = MP_SFTPUTILS.make_SFTPClient(vm->ssh_hostname(), vm->ssh_port(), vm->ssh_username(),
                                                    ssh_key_provider->private_key_as_base64());
    std::istringstream creds_stringstream(smb_creds);
    sftp_client->from_cin(creds_stringstream, credentials_path, false);

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
    {
        remove_cred_files(user_id);
        throw std::runtime_error(fmt::format("Error: {}", mount_proc.read_std_error()));
    }
}
catch (...)
{
    remove_smb_share();
    throw;
}

void SmbMountHandler::deactivate_impl(bool force)
try
{
    mpl::log(mpl::Level::info, category,
             fmt::format("Stopping native mount \"{}\" in instance '{}'", target, vm->vm_name));
    SSHSession session{vm->ssh_hostname(), vm->ssh_port(), vm->ssh_username(), *ssh_key_provider};
    mpu::run_in_ssh_session(session, fmt::format("if mountpoint -q {0}; then sudo umount {0}; else true; fi", target));
    remove_smb_share();
}
catch (const std::exception& e)
{
    if (!force)
        throw;
    mpl::log(mpl::Level::warning, category,
             fmt::format("Failed to gracefully stop mount \"{}\" in instance '{}': {}", target, vm->vm_name, e.what()));
    remove_smb_share();
}

SmbMountHandler::~SmbMountHandler()
{
    deactivate(/*force=*/true);
}
} // namespace multipass
