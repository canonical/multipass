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

#include <multipass/exceptions/exitless_sshprocess_exceptions.h>
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

namespace
{
constexpr auto category = "smb-mount-handler";

void install_cifs_for(const std::string& name,
                      mp::SSHSession& session,
                      const std::chrono::milliseconds& timeout)
try
{
    mpl::info(category, "Installing cifs-utils in '{}'", name);

    auto proc = session.exec("sudo apt-get update && sudo apt-get install -y cifs-utils");
    if (proc.exit_code(timeout) != 0)
    {
        auto error_msg = proc.read_std_error();
        mpl::warn(category,
                  "Failed to install 'cifs-utils', error message: '{}'",
                  mp::utils::trim_end(error_msg));
        throw std::runtime_error("Failed to install cifs-utils");
    }
}
catch (const mp::ExitlessSSHProcessException&)
{
    mpl::info(category, "Timeout while installing 'cifs-utils' in '{}'", name);
    throw std::runtime_error("Timeout installing cifs-utils");
}
} // namespace

namespace multipass
{
bool SmbManager::share_exists(const QString& share_name) const
{
    PSHARE_INFO_0 share_info;
    auto wide_share_name = share_name.toStdWString();
    const auto res = NetShareGetInfo(nullptr, wide_share_name.data(), 0, (LPBYTE*)&share_info);
    NetApiBufferFree(share_info);
    return res == 0;
}

void SmbManager::create_share(const QString& share_name,
                              const QString& source,
                              const QString& user) const
{
    if (share_exists(share_name))
        return;

    // TODO: I tried to use the proper Windows API to get ACL permissions for the user being passed
    // in, but alas, the API is very convoluted. At some point, another attempt should be made to
    // use the proper API though...
    QString user_access_output;
    const auto user_access_res =
        PowerShell::exec({QString{"(Get-Acl '%1').Access | ?{($_.IdentityReference -match '%2') "
                                  "-and ($_.FileSystemRights "
                                  "-eq 'FullControl')}"}
                              .arg(source, user)},
                         "Get ACLs",
                         &user_access_output);

    if (!user_access_res || user_access_output.isEmpty())
        throw std::runtime_error{fmt::format("cannot access \"{}\"", source)};

    std::wstring remark = L"Multipass mount share";
    auto wide_share_name = share_name.toStdWString();
    auto wide_source = source.toStdWString();

    DWORD parm_err = 0;
    SHARE_INFO_2 share_info;
    share_info.shi2_netname = wide_share_name.data();
    share_info.shi2_remark = remark.data();
    share_info.shi2_type = STYPE_DISKTREE;
    share_info.shi2_permissions = 0;
    share_info.shi2_max_uses = -1;
    share_info.shi2_current_uses = 0;
    share_info.shi2_path = wide_source.data();
    share_info.shi2_passwd = nullptr;

    if (NetShareAdd(nullptr, 2, (LPBYTE)&share_info, &parm_err) != 0)
        throw std::runtime_error{
            fmt::format("failed creating SMB share for \"{}\": {}", source, parm_err)};
}

void SmbManager::remove_share(const QString& share_name) const
{
    auto wide_share_name = share_name.toStdWString();
    if (share_exists(share_name) && NetShareDel(nullptr, wide_share_name.data(), 0) != 0)
        mpl::warn(category, "Failed removing SMB share \"{}\"'", share_name);
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

void SmbMountHandler::encrypt_credentials_to_file(const QString& cred_filename,
                                                  const QString& iv_filename,
                                                  const std::string& data)
try
{
    const auto iv = MP_UTILS.random_bytes(MP_AES.aes_256_block_size());
    const auto encrypted_data = MP_AES.encrypt(enc_key, iv, data);

    MP_UTILS.make_file_with_content(cred_dir.filePath(iv_filename).toStdString(),
                                    {iv.begin(), iv.end()});
    MP_UTILS.make_file_with_content(cred_dir.filePath(cred_filename).toStdString(),
                                    {encrypted_data.begin(), encrypted_data.end()});

    mpl::info(category, "Successfully encrypted credentials");
}
catch (const std::exception& e)
{
    mpl::warn(category, "Failed to encrypt credentials to file: {}", e.what());
}

std::string SmbMountHandler::decrypt_credentials_from_file(const QString& cred_filename,
                                                           const QString& iv_filename)
try
{
    const auto encrypted_data = MP_UTILS.contents_of(cred_dir.filePath(cred_filename));
    const auto iv_str = MP_UTILS.contents_of(cred_dir.filePath(iv_filename));
    std::vector<uint8_t> iv{iv_str.begin(), iv_str.end()};
    iv.resize(MP_AES.aes_256_block_size());

    auto decrypted_data = MP_AES.decrypt(enc_key, iv, encrypted_data);
    mpl::info(category, "Successfully decrypted credentials");
    return decrypted_data;
}
catch (const std::exception& e)
{
    mpl::warn(category, "Failed to decrypt credentials from file: {}", e.what());
    return {};
}

SmbMountHandler::SmbMountHandler(VirtualMachine* vm,
                                 const SSHKeyProvider* ssh_key_provider,
                                 const std::string& target,
                                 VMMount mount_spec,
                                 const mp::Path& cred_dir,
                                 const SmbManager& smb_manager)
    : MountHandler{vm, ssh_key_provider, std::move(mount_spec), target},
      source{QString::fromStdString(get_mount_spec().get_source_path())},
      // share name must be unique and 80 chars max
      // UUIDS are 36 chars each, and +1 for dash: 73 characters.
      share_name{QString::fromStdString(
          fmt::format("{}-{}", MP_UTILS.make_uuid(vm->vm_name), MP_UTILS.make_uuid(target)))},
      cred_dir{cred_dir},
      smb_manager{&smb_manager}
{
    mpl::info(category, "Initializing native mount {} => {} in '{}'", source, target, vm->vm_name);

    auto data_location{MP_PLATFORM.multipass_storage_location() + "\\data"};
    auto enc_key_dir_path{MP_UTILS.make_dir(data_location, "enc-keys")};
    auto key_file = QDir{enc_key_dir_path}.filePath("aes.key");

    if (MP_FILEOPS.exists(QFile{key_file}))
    {
        const auto key_str = MP_UTILS.contents_of(key_file);
        enc_key.assign(key_str.begin(), key_str.end());
        enc_key.resize(MP_AES.aes_256_key_size());
    }
    else
    {
        enc_key = MP_UTILS.random_bytes(MP_AES.aes_256_key_size());
        MP_UTILS.make_file_with_content(key_file.toStdString(), {enc_key.begin(), enc_key.end()});
        mpl::info(category, "Successfully generated new encryption key");
    }
}

bool SmbMountHandler::is_active()
try
{
    return active && smb_manager->share_exists(share_name) &&
           !SSHSession{vm->ssh_hostname(), vm->ssh_port(), vm->ssh_username(), *ssh_key_provider}
                .exec(fmt::format("findmnt --type cifs | grep '{} //{}/{}'",
                                  target,
                                  QHostInfo::localHostName(),
                                  share_name))
                .exit_code();
}
catch (const std::exception& e)
{
    mpl::warn(category,
              "Failed checking SSHFS mount \"{}\" in instance '{}': {}",
              target,
              vm->vm_name,
              e.what());
    return false;
}

void SmbMountHandler::activate_impl(ServerVariant server, std::chrono::milliseconds timeout)
try
{
    SSHSession session{vm->ssh_hostname(), vm->ssh_port(), vm->ssh_username(), *ssh_key_provider};

    const auto username = MP_PLATFORM.get_username();
    const auto user_id = MP_UTILS.make_uuid(username.toStdString());
    const auto iv_filename = user_id + ".iv";
    const auto cred_filename = user_id + ".cifs";

    if (session.exec("dpkg-query --show --showformat='${db:Status-Status}' cifs-utils")
            .read_std_output() != "installed")
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
            [](auto&& server) {
                auto reply =
                    server
                        ? make_reply_from_server(server)
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

        encrypt_credentials_to_file(cred_filename,
                                    iv_filename,
                                    fmt::format("password={}", password));
    }

    smb_manager->create_share(share_name, source, username);

    // The following mkdir in the instance will be replaced with refactored code
    auto mkdir_proc = session.exec(fmt::format("mkdir -p {}", target));
    if (mkdir_proc.exit_code() != 0)
        throw std::runtime_error(fmt::format("Cannot create \"{}\" in instance '{}': {}",
                                             target,
                                             vm->vm_name,
                                             mkdir_proc.read_std_error()));

    auto smb_creds = fmt::format("username={}\npassword={}", username, password);
    const std::string credentials_path{"/tmp/.smb_credentials"};
    auto sftp_client = MP_SFTPUTILS.make_SFTPClient(vm->ssh_hostname(),
                                                    vm->ssh_port(),
                                                    vm->ssh_username(),
                                                    ssh_key_provider->private_key_as_base64());
    std::istringstream creds_stringstream(smb_creds);
    sftp_client->from_cin(creds_stringstream, credentials_path, false);

    auto hostname = QHostInfo::localHostName();
    auto mount_proc = session.exec(
        fmt::format("sudo mount -t cifs //{}/{} {} -o credentials={},uid=$(id -u),gid=$(id -g)",
                    hostname,
                    share_name,
                    target,
                    credentials_path));
    auto mount_exit_code = mount_proc.exit_code();
    auto mount_error_msg = mount_proc.read_std_error();

    auto rm_proc = session.exec(fmt::format("sudo rm {}", credentials_path));
    if (rm_proc.exit_code() != 0)
        mpl::warn(category,
                  "Failed deleting credentials file in \'{}\': {}",
                  vm->vm_name,
                  rm_proc.read_std_error());

    if (mount_exit_code != 0)
    {
        remove_cred_files(user_id);
        throw std::runtime_error(fmt::format("Error: {}", mount_error_msg));
    }
}
catch (...)
{
    smb_manager->remove_share(share_name);
    throw;
}

void SmbMountHandler::deactivate_impl(bool force)
try
{
    mpl::info(category, "Stopping native mount \"{}\" in instance '{}'", target, vm->vm_name);
    SSHSession session{vm->ssh_hostname(), vm->ssh_port(), vm->ssh_username(), *ssh_key_provider};
    MP_UTILS.run_in_ssh_session(
        session,
        fmt::format("if mountpoint -q {0}; then sudo umount {0}; else true; fi", target));
    smb_manager->remove_share(share_name);
}
catch (const std::exception& e)
{
    if (!force)
        throw;
    mpl::warn(category,
              "Failed to gracefully stop mount \"{}\" in instance '{}': {}",
              target,
              vm->vm_name,
              e.what());
    smb_manager->remove_share(share_name);
}

SmbMountHandler::~SmbMountHandler()
{
    deactivate(/*force=*/true);
}
} // namespace multipass
