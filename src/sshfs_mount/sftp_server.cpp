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

#include "sftp_server.h"

#include <multipass/cli/client_platform.h>
#include <multipass/exceptions/exitless_sshprocess_exceptions.h>
#include <multipass/exceptions/ssh_exception.h>
#include <multipass/logging/log.h>
#include <multipass/logging/log_location.h>
#include <multipass/platform.h>
#include <multipass/ssh/ssh_session.h>
#include <multipass/ssh/throw_on_error.h>
#include <multipass/utils/permission_utils.h>

#include <multipass/utils.h>

extern "C"
{
int sftp_reply_version(sftp_client_message msg);
}

#include <QDir>
#include <QFile>

#include <fcntl.h>

namespace mp = multipass;
namespace mpl = multipass::logging;
namespace fs = std::filesystem;
using fsp = fs::perms;

namespace
{
constexpr auto category = "sftp server";
using SftpHandleUPtr = std::unique_ptr<ssh_string_struct, void (*)(ssh_string)>;
using namespace std::literals::chrono_literals;

// TODO: wstring support for Windows

auto make_sftp_session(ssh_session session, ssh_channel channel)
{
    mp::SftpServer::SftpSessionUptr sftp_server_session{sftp_server_new(session, channel),
                                                        sftp_server_free};
    // The function sftp_server_init was expanded here to avoid deprecation warnings.
    // TODO: move to callback-based sftp implementations.
    // https://github.com/canonical/multipass/issues/4445

    /* handles setting the sftp->client_version */
    sftp_client_message msg{sftp_get_client_message(sftp_server_session.get())};
    if (msg == nullptr)
    {
        throw mp::SSHException("[sftp] server init failed: 'Null client message'");
    }

    if (msg->type != SSH_FXP_INIT)
    {
        throw mp::SSHException(fmt::format(
            "[sftp] server init failed: 'FATAL: Packet read of type {} instead of SSH_FXP_INIT'",
            msg->type));
    }

    // Optional: Log the SSH_FXP_INIT reception like libssh does with SSH_LOG but with mp::log

    if (sftp_reply_version(msg) != SSH_OK)
    {
        throw mp::SSHException(
            "[sftp] server init failed: 'FATAL: Failed to process the SSH_FXP_INIT message'");
    }

    return sftp_server_session;
}

int reply_ok(sftp_client_message msg)
{
    return sftp_reply_status(msg, SSH_FX_OK, nullptr);
}

int reply_failure(sftp_client_message msg)
{
    return sftp_reply_status(msg, SSH_FX_FAILURE, nullptr);
}

int reply_perm_denied(sftp_client_message msg)
{
    return sftp_reply_status(msg, SSH_FX_PERMISSION_DENIED, "permission denied");
}

int reply_bad_handle(sftp_client_message msg, const char* type)
{
    return sftp_reply_status(msg,
                             SSH_FX_BAD_MESSAGE,
                             fmt::format("{}: invalid handle", type).c_str());
}

int reply_unsupported(sftp_client_message msg)
{
    return sftp_reply_status(msg, SSH_FX_OP_UNSUPPORTED, "Unsupported message");
}

fmt::memory_buffer& operator<<(fmt::memory_buffer& buf, const char* v)
{
    fmt::format_to(std::back_inserter(buf), "{}", v);
    return buf;
}

bool is_directory(const sftp_attributes_struct& attr)
{
    return (attr.permissions & SSH_S_IFMT) == SSH_S_IFDIR;
}

bool is_symlink(const sftp_attributes_struct& attr)
{
    return (attr.permissions & SSH_S_IFMT) == SSH_S_IFLNK;
}

auto longname_from(const sftp_attributes_struct& file_attr, const std::string& filename)
{
    fmt::memory_buffer out;

    if (is_symlink(file_attr))
        out << "l";
    else if (is_directory(file_attr))
        out << "d";
    else
        out << "-";

    fsp perms = static_cast<fsp>(file_attr.permissions);
    /* user */
    if ((perms & fsp::owner_read) != fsp::none)
        out << "r";
    else
        out << "-";

    if ((perms & fsp::owner_write) != fsp::none)
        out << "w";
    else
        out << "-";

    if ((perms & fsp::owner_exec) != fsp::none)
        out << "x";
    else
        out << "-";

    /*group*/
    if ((perms & fsp::group_read) != fsp::none)
        out << "r";
    else
        out << "-";

    if ((perms & fsp::group_write) != fsp::none)
        out << "w";
    else
        out << "-";

    if ((perms & fsp::group_exec) != fsp::none)
        out << "x";
    else
        out << "-";

    /* other */
    if ((perms & fsp::others_read) != fsp::none)
        out << "r";
    else
        out << "-";

    if ((perms & fsp::others_write) != fsp::none)
        out << "w";
    else
        out << "-";

    if ((perms & fsp::others_exec) != fsp::none)
        out << "x";
    else
        out << "-";

    fmt::format_to(std::back_inserter(out),
                   " 1 {} {} {}",
                   file_attr.uid,
                   file_attr.gid,
                   file_attr.size);

    const auto timestamp = MP_UTILS.format_time_t(file_attr.mtime);
    fmt::format_to(std::back_inserter(out), " {} {}", timestamp, filename);

    return out;
}

void check_sshfs_status(mp::SSHSession& session, mp::SSHProcess& sshfs_process)
{
    if (sshfs_process.exit_recognized(250ms))
    {
        // This `if` is artificial and should not really be here. However there is a complex
        // arrangement of Sftp and SshfsMount tests depending on this.
        if (sshfs_process.exit_code(250ms) != 0) // TODO remove
            throw std::runtime_error(sshfs_process.read_std_error());
    }
}

auto create_sshfs_process(mp::SSHSession& session,
                          const std::string& sshfs_exec_line,
                          const std::string& source,
                          const std::string& target)
{
    auto sshfs_process =
        session.exec(fmt::format("sudo {} :{:?} {:?}", sshfs_exec_line, source, target));

    check_sshfs_status(session, sshfs_process);

    return std::make_unique<mp::SSHProcess>(std::move(sshfs_process));
}

int mapped_id_for(const mp::id_mappings& id_maps, const int id, const int default_id)
{
    if (id == mp::no_id_info_available)
        return default_id;

    auto found = std::find_if(id_maps.cbegin(), id_maps.cend(), [id](std::pair<int, int> p) {
        return id == p.first;
    });

    return found == id_maps.cend() ? -1
                                   : (found->second == mp::default_id ? default_id : found->second);
}

int reverse_id_for(const mp::id_mappings& id_maps, const int id, const int default_id)
{
    auto found = std::find_if(id_maps.cbegin(), id_maps.cend(), [id](std::pair<int, int> p) {
        return id == p.second;
    });
    auto default_found =
        std::find_if(id_maps.cbegin(), id_maps.cend(), [default_id](std::pair<int, int> p) {
            return default_id == p.second;
        });

    return found == id_maps.cend()
             ? (default_found == id_maps.cend() ? default_id : default_found->first)
             : found->first;
}

constexpr bool request_requires_file_exists(uint8_t type)
{
    switch (type)
    {
    case SFTP_LSTAT:
    case SFTP_READLINK:
    case SFTP_REMOVE:
    case SFTP_RMDIR:
    case SFTP_MKDIR:
    case SFTP_RENAME:
    case SFTP_SYMLINK:
        return false;
    case SFTP_OPEN:
    case SFTP_OPENDIR:
    case SFTP_REALPATH:
    case SFTP_SETSTAT:
    case SFTP_STAT:
    case SFTP_EXTENDED:
    default:
        return true; // Fail-safe default
    }
}
} // namespace

mp::SftpServer::SftpServer(SSHSession&& session,
                           const std::string& source,
                           const std::string& target,
                           const id_mappings& gid_mappings,
                           const id_mappings& uid_mappings,
                           int default_uid,
                           int default_gid,
                           const std::string& sshfs_exec_line)
    : ssh_session{std::move(session)},
      sshfs_process{create_sshfs_process(ssh_session, sshfs_exec_line, source, target)},
      sftp_server_session{make_sftp_session(ssh_session, sshfs_process->release_channel())},
      source_path{MP_FILEOPS.weakly_canonical(source)},
      target_path{mp::utils::normalize_path(fs::path(target))},
      gid_mappings{gid_mappings},
      uid_mappings{uid_mappings},
      default_uid{default_uid},
      default_gid{default_gid},
      sshfs_exec_line{sshfs_exec_line}
{
}

mp::SftpServer::~SftpServer()
{
    stop_invoked = true;
}

void mp::SftpServer::convert_attr_ids(sftp_attributes_struct& attr)
{
    attr.uid = mapped_uid_for(attr.uid);
    attr.gid = mapped_gid_for(attr.gid);
}

inline int mp::SftpServer::mapped_uid_for(const int uid)
{
    return mapped_id_for(uid_mappings, uid, default_uid);
}

inline int mp::SftpServer::mapped_gid_for(const int gid)
{
    return mapped_id_for(gid_mappings, gid, default_gid);
}

inline int mp::SftpServer::reverse_uid_for(const int uid, const int default_id)
{
    return reverse_id_for(uid_mappings, uid, default_id);
}

inline int mp::SftpServer::reverse_gid_for(const int gid, const int default_id)
{
    return reverse_id_for(gid_mappings, gid, default_id);
}

inline bool mp::SftpServer::has_uid_mapping_for(const int uid)
{
    return std::any_of(uid_mappings.begin(), uid_mappings.end(), [uid](std::pair<int, int> p) {
        return uid == p.first;
    });
}

inline bool mp::SftpServer::has_gid_mapping_for(const int gid)
{
    return std::any_of(gid_mappings.begin(), gid_mappings.end(), [gid](std::pair<int, int> p) {
        return gid == p.first;
    });
}

inline bool mp::SftpServer::has_reverse_uid_mapping_for(const int uid)
{
    return std::any_of(uid_mappings.cbegin(), uid_mappings.cend(), [uid](std::pair<int, int> p) {
        return uid == p.second;
    });
}

inline bool mp::SftpServer::has_reverse_gid_mapping_for(const int gid)
{
    return std::any_of(gid_mappings.cbegin(), gid_mappings.cend(), [gid](std::pair<int, int> p) {
        return gid == p.second;
    });
}

bool mp::SftpServer::has_id_mappings_for(const sftp_attributes_struct& file_attr)
{
    return has_uid_mapping_for(file_attr.uid) && has_gid_mapping_for(file_attr.gid);
}

bool mp::SftpServer::validate_path(const fs::path& current_path, bool check_file_itself) const
{
    if (source_path.empty() || current_path.empty())
        return false;

    fs::path final_path;
    try
    {

        // weakly_canonical allows paths that do not exist. This means that broken links will be
        // treated as literal folders/files, so they need to be filtered out.
        fs::path check_path = check_file_itself ? current_path : current_path.parent_path();
        if (check_path.empty())
        {
            check_path = ".";
        }
        while (check_path != check_path.parent_path() && !MP_FILEOPS.exists(check_path))
        {
            if (MP_FILEOPS.is_symlink(check_path))
            {
                // A broken symlink was detected! It could point anywhere on the host.
                return false;
            }
            check_path = check_path.parent_path();
        }
        // If no broken symlinks, canonicalize the path.
        if (check_file_itself)
            final_path = MP_FILEOPS.weakly_canonical(current_path);
        else
        {
            // The target path is a symlink, we examine the parent path
            auto resolved_parent = MP_FILEOPS.weakly_canonical(current_path.parent_path());
            // If no broken symlinks, canonicalize the path.
            final_path = mp::utils::normalize_path(resolved_parent / current_path.filename());
        }
    }
    catch (const fs::filesystem_error& e)
    {
        mpl::trace(category, "Could not resolve path {} : {}", current_path.string(), e.what());
        return false;
    }

    auto [source_it, current_it] =
        std::mismatch(source_path.begin(), source_path.end(), final_path.begin(), final_path.end());
    return source_it == source_path.end();
}

fs::path mp::SftpServer::get_absolute_path(const char* path) const
{
    fs::path raw = path != nullptr ? fs::path(path) : fs::path();
    if (raw.is_relative() && !raw.empty())
    {
        return source_path / raw;
    }
    return raw;
}

std::optional<fs::path> mp::SftpServer::get_validated_path(sftp_client_message msg) const
{
    bool check_file_exists{request_requires_file_exists(sftp_client_message_get_type(msg))};
    const auto path = get_absolute_path(sftp_client_message_get_filename(msg));
    if (!validate_path(path, check_file_exists))
    {
        mpl::trace_location(category,
                            "cannot validate path '{}' against source '{}'",
                            path.string(),
                            source_path.string());
        return std::nullopt;
    }
    return path;
}

std::string mp::SftpServer::host_to_guest_path(const fs::path& host_path) const
{
    std::error_code ec;
    // Get the relative difference between the host path and the mount root
    auto relative = MP_FILEOPS.relative(host_path, source_path, ec);

    if (ec || relative.empty() || relative == "." || relative.begin()->string() == ".." ||
        relative.is_absolute() || relative.has_root_name())
    {
        // If the path is outside the mount or invalid, return the mount path
        return target_path.generic_string();
    }

    // Return it as an absolute path from the perspective of the guest
    return mp::utils::normalize_path(target_path / relative).generic_string();
}

void mp::SftpServer::process_message(sftp_client_message msg)
{
    int ret = 0;
    const auto type = sftp_client_message_get_type(msg);
    switch (type)
    {
    case SFTP_REALPATH:
        ret = handle_realpath(msg);
        break;
    case SFTP_OPENDIR:
        ret = handle_opendir(msg);
        break;
    case SFTP_MKDIR:
        ret = handle_mkdir(msg);
        break;
    case SFTP_RMDIR:
        ret = handle_rmdir(msg);
        break;
    case SFTP_LSTAT:
    case SFTP_STAT:
        ret = handle_stat(msg, type == SFTP_STAT);
        break;
    case SFTP_FSTAT:
        ret = handle_fstat(msg);
        break;
    case SFTP_READDIR:
        ret = handle_readdir(msg);
        break;
    case SFTP_CLOSE:
        ret = handle_close(msg);
        break;
    case SFTP_OPEN:
        ret = handle_open(msg);
        break;
    case SFTP_READ:
        ret = handle_read(msg);
        break;
    case SFTP_WRITE:
        ret = handle_write(msg);
        break;
    case SFTP_RENAME:
        ret = handle_rename(msg);
        break;
    case SFTP_REMOVE:
        ret = handle_remove(msg);
        break;
    case SFTP_SETSTAT:
    case SFTP_FSETSTAT:
        ret = handle_setstat(msg);
        break;
    case SFTP_READLINK:
        ret = handle_readlink(msg);
        break;
    case SFTP_SYMLINK:
        ret = handle_symlink(msg);
        break;
    case SFTP_EXTENDED:
        ret = handle_extended(msg);
        break;
    default:
        mpl::trace(category, "Unknown message: {}", static_cast<int>(type));
        ret = reply_unsupported(msg);
    }
    if (ret != 0)
        mpl::error(category, "error occurred when replying to client: {}", ret);
}

void mp::SftpServer::run()
{
    using MsgUPtr =
        std::unique_ptr<sftp_client_message_struct, decltype(sftp_client_message_free)*>;

    while (true)
    {
        MsgUPtr client_msg{sftp_get_client_message(sftp_server_session.get()),
                           sftp_client_message_free};
        auto msg = client_msg.get();
        if (msg == nullptr)
        {
            if (stop_invoked)
                break;

            int status{0};
            try
            {
                status = sshfs_process->exit_code(250ms);
            }
            catch (const mp::ExitlessSSHProcessException&) // should we limit this to
                                                           // SSHProcessExitError?
            {
                status = 1;
            }

            if (status != 0)
            {
                mpl::error(category,
                           "sshfs in the instance appears to have exited unexpectedly.  Trying to "
                           "recover.");

                std::string mount_path = [this] {
                    auto proc = ssh_session.exec(
                        fmt::format("findmnt --source :{}  -o TARGET -n", source_path));
                    return proc.read_std_output();
                }();

                if (!mount_path.empty())
                {
                    ssh_session.exec(fmt::format("sudo umount {}", mount_path));
                }

                sshfs_process = create_sshfs_process(ssh_session,
                                                     sshfs_exec_line,
                                                     source_path.string(),
                                                     target_path.generic_string());
                sftp_server_session =
                    make_sftp_session(ssh_session, sshfs_process->release_channel());

                continue;
            }
            else
            {
                break;
            }
        }

        // Expectation is that there are no unmanaged exceptions at the handle_* methods
        process_message(msg);
    }
}

void mp::SftpServer::stop()
{
    stop_invoked = true;
    ssh_session.force_shutdown();
}

int mp::SftpServer::handle_close(sftp_client_message msg)
{
    const auto id = sftp_handle(sftp_server_session.get(), msg->handle);
    if (!open_file_handles.erase(id) && !open_dir_handles.erase(id))
    {
        mpl::trace_location(category, "bad handle requested");
        return reply_bad_handle(msg, "close");
    }

    sftp_handle_remove(sftp_server_session.get(), id);
    return reply_ok(msg);
}

int mp::SftpServer::handle_fstat(sftp_client_message msg)
{
    const auto handle = get_handle<NamedFd>(msg);
    if (handle == nullptr)
    {
        mpl::trace_location(category, "bad handle requested");
        return reply_bad_handle(msg, "fstat");
    }

    const auto& [path, fd] = *handle;

    // File is supposed to be open, and as such it must exist
    // No need to verify path, we have an fd
    // if (!validate_path(path, request_requires_file_exists(SFTP_FSTAT)))
    //{
    //    mpl::trace_location(category,
    //                        "cannot validate target path \'{}\' against source \'{}\'",
    //                        path,
    //                        source_path);
    //    return reply_perm_denied(msg);
    //}

    // QFileInfo file_info(path);

    // if (file_info.isSymLink())
    //     file_info = QFileInfo(file_info.symLinkTarget());

    sftp_attributes_struct attr{};
    if (mp::platform::fstat_attr_from(fd, attr) != 0)
    {
        mpl::trace_location(category, "fstat failed: {}", std::strerror(errno));
        return reply_failure(msg);
    }
    convert_attr_ids(attr);
    return sftp_reply_attr(msg, &attr);
}

int mp::SftpServer::handle_mkdir(sftp_client_message msg)
{
    // The SFTP specification does not put its foot down on recursiveness,
    // but most implementations replicate the mkdir() syscall (non-recursive)
    // TODO: handle OS API errors differently for logging (lstat, mkdir)
    const auto filename = get_validated_path(msg);
    if (!filename.has_value())
        return reply_perm_denied(msg);

    sftp_attributes_struct parent_attr{};
    if (mp::platform::lstat_attr_from(filename->parent_path().string().c_str(), parent_attr) != 0)
    {
        mpl::trace_location(category,
                            "Parent path {} lstat failed, aborting mkdir on {}",
                            filename->parent_path().string(),
                            filename->string());
        return reply_failure(msg);
    }

    if (!has_id_mappings_for(parent_attr))
    {
        mpl::trace_location(
            category,
            "cannot create path \'{}\' with permissions \'{}:{}\': permission denied",
            parent_attr.uid,
            parent_attr.gid,
            filename->string());
        return reply_perm_denied(msg);
    }

    std::error_code ec;
    if (!MP_FILEOPS.create_directory(*filename, ec) || ec)
    {
        // If the directory exists, return failure
        mpl::trace_location(category, "mkdir failed for '{}'", filename->string());
        return reply_failure(msg);
    }

    if (!MP_PLATFORM.set_permissions(*filename, static_cast<fs::perms>(msg->attr->permissions)))
    {
        mpl::trace_location(category, "set permissions failed for '{}'", filename->string());
        return reply_failure(msg);
    }

    int rev_uid = reverse_uid_for(parent_attr.uid, parent_attr.uid);
    int rev_gid = reverse_gid_for(parent_attr.gid, parent_attr.gid);

    if (MP_PLATFORM.chown(filename->string().c_str(), rev_uid, rev_gid) < 0)
    {
        mpl::trace(category,
                   "failed to chown '{}' to owner:{} and group:{}",
                   filename->string(),
                   rev_uid,
                   rev_gid);
        return reply_failure(msg);
    }

    return reply_ok(msg);
}

int mp::SftpServer::handle_rmdir(sftp_client_message msg)
{
    // The sftp specification says that only directories should be removed with this message,
    // similarly to ::rmdir()
    const auto filename = get_validated_path(msg);
    if (!filename.has_value())
        return reply_perm_denied(msg);

    sftp_attributes_struct attr{};
    if (mp::platform::lstat_attr_from(filename->string().c_str(), attr) != 0)
    {
        mpl::trace_location(category, "rmdir failed for '{}': does not exist", filename->string());
        return reply_failure(msg);
    }
    if (!is_directory(attr))
    {
        mpl::trace_location(category, "path \'{}\' is not a directory", filename->string());
        return reply_failure(msg);
    }
    if (!has_id_mappings_for(attr))
    {
        mpl::trace_location(category,
                            "cannot access path \'{}\' without id mapping: permission denied",
                            filename->string());
        return reply_perm_denied(msg);
    }

    std::error_code err;
    if (!MP_FILEOPS.remove(*filename, err) && !err)
        err = std::make_error_code(std::errc::no_such_file_or_directory);

    if (err)
    {
        mpl::trace_location(category,
                            "rmdir failed for '{}': {}",
                            filename->string(),
                            err.message());
        return reply_failure(msg);
    }

    return reply_ok(msg);
}

int mp::SftpServer::handle_open(sftp_client_message msg)
{
    const auto filename = get_validated_path(msg);
    if (!filename.has_value())
        return reply_perm_denied(msg);

    bool exists{true};
    sftp_attributes_struct attr{};
    exists = mp::platform::lstat_attr_from(filename->string().c_str(), attr) == 0;
    if (!exists)
    {
        // If it does not exist, we can continue, otherwise it is an error
        if (errno != ENOENT)
        {
            mpl::trace(category,
                       "Cannot get status of '{}': {}",
                       filename->string(),
                       std::strerror(errno));
            return reply_perm_denied(msg);
        }
        sftp_attributes_struct dir_attr{};
        if (mp::platform::stat_attr_from(filename->parent_path().string().c_str(), dir_attr) != 0 ||
            !has_id_mappings_for(dir_attr))
        {
            mpl::trace_location(
                category,
                "cannot access parent path '{}' without id mapping: permission denied",
                filename->parent_path().string());
            return reply_perm_denied(msg);
        }
        attr = dir_attr;
    }
    else
    {
        if (!has_id_mappings_for(attr))
        {
            mpl::trace_location(category,
                                "cannot access path '{}' without id mapping: permission denied",
                                filename->string());
            return reply_perm_denied(msg);
        }
    }

    int mode = 0;
    const auto flags = sftp_client_message_get_flags(msg);

    if ((flags & SSH_FXF_READ) && (flags & SSH_FXF_WRITE))
        mode |= O_RDWR;
    else if (flags & SSH_FXF_READ)
        mode |= O_RDONLY;
    else if (flags & SSH_FXF_WRITE)
        mode |= O_WRONLY;

    if (flags & SSH_FXF_APPEND)
        mode |= O_APPEND;

    if (flags & SSH_FXF_TRUNC)
        mode |= O_TRUNC;

    if (flags & SSH_FXF_CREAT)
        mode |= O_CREAT;

    if (flags & SSH_FXF_EXCL)
        mode |= O_EXCL;

    auto named_fd_handle =
        MP_FILEOPS.open_fd(*filename, mode, msg->attr ? msg->attr->permissions : 0);
    auto named_fd = std::get_if<NamedFd>(named_fd_handle.get());
    if (!named_fd || named_fd->fd == -1)
    {
        mpl::trace(category, "Cannot open '{}': {}", filename->string(), std::strerror(errno));
        return reply_failure(msg);
    }

    if (!exists)
    {
        auto new_uid = reverse_uid_for(attr.uid, attr.uid);
        auto new_gid = reverse_gid_for(attr.gid, attr.gid);

        if (MP_PLATFORM.fchown(named_fd->fd, new_uid, new_gid) < 0)
        {
            mpl::trace(category,
                       "failed to chown '{}' to owner:{} and group:{}",
                       filename->string(),
                       new_uid,
                       new_gid);
            return reply_failure(msg);
        }
    }

    SftpHandleUPtr sftp_handle{sftp_handle_alloc(sftp_server_session.get(), named_fd_handle.get()),
                               ssh_string_free};
    if (!sftp_handle)
    {
        mpl::trace(category, "Cannot allocate handle for open()");
        return reply_failure(msg);
    }

    auto fd_key_ptr{named_fd_handle.get()};
    open_file_handles.emplace(fd_key_ptr, std::move(named_fd_handle));

    return sftp_reply_handle(msg, sftp_handle.get());
}

int mp::SftpServer::handle_opendir(sftp_client_message msg)
{
    const auto filename = get_validated_path(msg);
    if (!filename.has_value())
        return reply_perm_denied(msg);

    std::error_code err;
    sftp_attributes_struct attr{};
    const auto res = mp::platform::stat_attr_from(filename->string().c_str(),
                                                  attr); // First stat to minimize TOCTOU impact
    auto dir_iterator = MP_FILEOPS.dir_iterator(*filename, err);

    if (res != 0 || err.value() == int(std::errc::no_such_file_or_directory) ||
        err.value() == int(std::errc::no_such_process))
    {
        mpl::trace(category, "Cannot open directory '{}': {}", filename->string(), err.message());
        return sftp_reply_status(msg, SSH_FX_NO_SUCH_FILE, "no such directory");
    }

    if (err.value() == int(std::errc::permission_denied))
    {
        mpl::trace(category, "Cannot read directory '{}': {}", filename->string(), err.message());
        return reply_perm_denied(msg);
    }

    if (!has_id_mappings_for(attr))
    {
        mpl::trace_location(category,
                            "cannot access path \'{}\' without id mapping: permission denied",
                            filename->string());
        return reply_perm_denied(msg);
    }

    SftpHandleUPtr sftp_handle{sftp_handle_alloc(sftp_server_session.get(), dir_iterator.get()),
                               ssh_string_free};
    if (!sftp_handle)
    {
        mpl::trace(category, "Cannot allocate handle for opendir()");
        return reply_failure(msg);
    }

    auto dir_iter_ptr{dir_iterator.get()};
    open_dir_handles.emplace(dir_iter_ptr, std::move(dir_iterator));

    return sftp_reply_handle(msg, sftp_handle.get());
}

int mp::SftpServer::handle_read(sftp_client_message msg)
{
    const auto handle = get_handle<NamedFd>(msg);
    if (handle == nullptr)
    {
        mpl::trace_location(category, "bad handle requested");
        return reply_bad_handle(msg, "read");
    }

    const auto& [path, fd] = *handle;

    constexpr auto max_packet_size = 65536u;
    // No array initialization. Memory does not leak because the message copy copies only the read
    // bytes.
    std::array<char, max_packet_size> buffer;
    const auto result =
        mp::platform::pread(fd, buffer.data(), std::min(msg->len, max_packet_size), msg->offset);

    if (result > 0)
        return sftp_reply_data(msg, buffer.data(), result);
    else if (result == 0)
        return sftp_reply_status(msg, SSH_FX_EOF, "End of file");

    mpl::trace_location(category, "read failed for '{}': {}", path.string(), std::strerror(errno));
    return sftp_reply_status(msg, SSH_FX_FAILURE, std::strerror(errno));
}

int mp::SftpServer::handle_readdir(sftp_client_message msg)
{
    const auto handle = get_handle<DirIterator>(msg);
    if (handle == nullptr)
    {
        mpl::trace_location(category, "bad handle requested");
        return reply_bad_handle(msg, "readdir");
    }

    auto& dir_iterator = *handle;

    if (!dir_iterator.hasNext())
        return sftp_reply_status(msg, SSH_FX_EOF, nullptr);

    const auto max_num_entries_per_packet = 50ll;
    for (int i = 0; i < max_num_entries_per_packet && dir_iterator.hasNext(); i++)
    {
        const auto& entry = dir_iterator.next();
        const auto& path = entry.path();
        const auto& path_string = path.string();
        sftp_attributes_struct attr{};

        if (mp::platform::lstat_attr_from(path_string.c_str(), attr) != 0)
        {
            mpl::trace(category,
                       "Cannot get status of '{}': {}",
                       path_string,
                       std::strerror(errno));
            return reply_failure(msg);
        }
        const auto longname = longname_from(attr, path);
        sftp_reply_names_add(msg, path.filename().string().c_str(), longname.data(), &attr);
    }

    return sftp_reply_names(msg);
}

int mp::SftpServer::handle_readlink(sftp_client_message msg)
{
    const auto filename = get_validated_path(msg);
    if (!filename.has_value())
        return reply_perm_denied(msg);

    sftp_attributes_struct attr{};
    if (mp::platform::lstat_attr_from(filename->string().c_str(), attr) != 0 ||
        !has_id_mappings_for(attr))
    {
        mpl::trace_location(category,
                            "cannot access path '{}' without id mapping: permission denied",
                            filename->string());
        return reply_perm_denied(msg);
    }

    std::error_code ec;
    // We give the raw stored link when reading, block on openat
    const auto raw_link = MP_FILEOPS.read_symlink(*filename, ec);
    if (ec)
    {
        mpl::trace_location(category, "invalid link for \'{}\'", filename->string());
        return sftp_reply_status(msg, SSH_FX_NO_SUCH_FILE, "invalid link");
    }

    sftp_attributes_struct dummy_attr{};
    // SFTP spec v3 states that dummy/empty attr is returned for SSH_FXP_READLINK responses
    sftp_reply_names_add(msg, raw_link.string().c_str(), raw_link.string().c_str(), &dummy_attr);
    return sftp_reply_names(msg);
}

int mp::SftpServer::handle_realpath(sftp_client_message msg)
{
    const auto filename = get_validated_path(msg);
    if (!filename.has_value())
        return reply_perm_denied(msg);

    sftp_attributes_struct attr{};

    // Check ID mappings, but gracefully handle ENOENT (file doesn't exist yet)
    if (mp::platform::lstat_attr_from(filename->string().c_str(), attr) == 0)
    {
        if (!has_id_mappings_for(attr))
        {
            mpl::trace_location(category,
                                "cannot access path '{}' without id mapping: permission denied",
                                filepath->string());
            return reply_perm_denied(msg);
        }
    }
    else if (errno != ENOENT)
    {
        // Fail securely on real errors (e.g., EACCES - parent directory is restricted)
        mpl::trace_location(category,
                            "lstat failed for '{}': {}",
                            filepath->string(),
                            std::strerror(errno));
        return reply_perm_denied(msg);
    }
    // Path is already absolute and resolved from get_validated_path
    // Client is already aware of paths in the host, no sense doing translation
    const auto& filepath_string = MP_FILEOPS.weakly_canonical(*filepath).string();
    return sftp_reply_name(msg, filepath_string.c_str(), nullptr);
}

int mp::SftpServer::handle_remove(sftp_client_message msg)
{
    // Per v3 specification, this should ONLY remove files, not directories or links
    // TODO: Moving to platform-specific would allow rmdir(), which does not require checking
    // existence or lstat
    const auto filename = get_validated_path(msg);
    if (!filename.has_value())
        return reply_perm_denied(msg);

    sftp_attributes_struct attr{};
    if (mp::platform::lstat_attr_from(filename->string().c_str(), attr) != 0)
    {
        mpl::trace_location(category, "cannot remove '{}': Does not exist", filename->string());
        return sftp_reply_status(msg, SSH_FX_NO_SUCH_FILE, "No such file");
    }
    // File exists
    if (!has_id_mappings_for(attr))
    {
        mpl::trace_location(category,
                            "cannot access path '{}' without id mapping: permission denied",
                            filename->string());
        return reply_perm_denied(msg);
    }

    // Enforce SFTP protocol. SSH_FXP_REMOVE must fail on directories.
    if (is_directory(attr))
    {
        mpl::trace_location(category,
                            "refusing to remove directory '{}' via remove()",
                            filename->string());

        return reply_failure(msg);
    }

    std::error_code err;
    if (!MP_FILEOPS.remove(*filename, err) && !err)
        err = std::make_error_code(std::errc::no_such_file_or_directory);

    if (err)
    {
        mpl::trace_location(category, "cannot remove '{}': {}", filename->string(), err.message());
        return reply_failure(msg);
    }

    return reply_ok(msg);
}

int mp::SftpServer::handle_rename(sftp_client_message msg)
{
    const auto source = get_validated_path(msg);
    if (!source.has_value())
        return reply_perm_denied(msg);

    QFileInfo source_info{*source};
    if (!source_info.isSymLink() && !MP_FILEOPS.exists(source_info))
    {
        mpl::trace_location(category, "cannot rename \'{}\': no such file", source->string());
        return sftp_reply_status(msg, SSH_FX_NO_SUCH_FILE, "no such file");
    }

    if (!has_id_mappings_for(source_info))
    {
        mpl::trace_location(category,
                            "cannot access path \'{}\' without id mapping: permission denied",
                            source->string());
        return reply_perm_denied(msg);
    }

    const auto target = get_absolute_path(sftp_client_message_get_data(msg));
    // Hardcode false: Renaming overwrites a target link, it does not follow it!
    if (!validate_path(target, false))
    {
        mpl::trace_location(category,
                            "cannot validate target path \'{}\' against source \'{}\'",
                            target.string(),
                            source_path.string());
        return reply_perm_denied(msg);
    }

    QFileInfo target_info{target};
    if (MP_FILEOPS.exists(target_info) && !has_id_mappings_for(target_info))
    {
        mpl::trace_location(category,
                            "cannot access path \'{}\' without id mapping: permission denied",
                            target.string());
        return reply_perm_denied(msg);
    }

    QFile target_file{target};
    if (MP_FILEOPS.exists(target_info))
    {
        if (!MP_FILEOPS.remove(target_file))
        {
            mpl::trace_location(category, "cannot remove \'{}\' for renaming", target.string());
            return reply_failure(msg);
        }
    }

    QFile source_file{*source};
    if (!MP_FILEOPS.rename(source_file, target.string().c_str()))
    {
        mpl::trace_location(category,
                            "failed renaming \'{}\' to \'{}\'",
                            source->string(),
                            target.string());
        return reply_failure(msg);
    }

    return reply_ok(msg);
}

int mp::SftpServer::handle_setstat(sftp_client_message msg)
{
    fs::path filename;

    if (sftp_client_message_get_type(msg) == SFTP_FSETSTAT)
    {
        const auto handle = get_handle<NamedFd>(msg);
        if (handle == nullptr)
        {
            mpl::trace_location(category, "bad handle requested");
            return reply_bad_handle(msg, "setstat");
        }

        const auto& [path, _] = *handle;
        filename = path;
    }
    else
    {
        const auto validated_filename = get_validated_path(msg);
        if (!validated_filename.has_value())
            return reply_perm_denied(msg);

        filename = *validated_filename;

        QFileInfo file_info{filename};
        if (!file_info.isSymLink() && !MP_FILEOPS.exists(file_info))
        {
            mpl::trace_location(category, "cannot setstat '{}': no such file", filename.string());
            return sftp_reply_status(msg, SSH_FX_NO_SUCH_FILE, "no such file");
        }
    }

    QFileInfo file_info{filename};
    if (!has_id_mappings_for(file_info))
    {
        mpl::trace_location(category,
                            "cannot access path \'{}\' without id mapping: permission denied",
                            filename.string());
        return reply_perm_denied(msg);
    }

    if (msg->attr->flags & SSH_FILEXFER_ATTR_SIZE)
    {
        QFile file{filename};
        if (!MP_FILEOPS.resize(file, msg->attr->size))
        {
            mpl::trace_location(category, "cannot resize '{}'", filename.string());
            return reply_failure(msg);
        }
    }

    if (msg->attr->flags & SSH_FILEXFER_ATTR_PERMISSIONS)
    {
        if (!MP_PLATFORM.set_permissions(filename, static_cast<fs::perms>(msg->attr->permissions)))
        {
            mpl::trace_location(category, "set permissions failed for '{}'", filename.string());
            return reply_failure(msg);
        }
    }

    if (msg->attr->flags & SSH_FILEXFER_ATTR_ACMODTIME)
    {
        if (MP_PLATFORM.utime(filename.string().c_str(), msg->attr->atime, msg->attr->mtime) < 0)
        {
            mpl::trace_location(category,
                                "cannot set modification date for '{}'",
                                filename.string());
            return reply_failure(msg);
        }
    }

    if (msg->attr->flags & SSH_FILEXFER_ATTR_UIDGID)
    {
        if (!has_reverse_uid_mapping_for(msg->attr->uid) &&
            !has_reverse_gid_mapping_for(msg->attr->gid))
        {
            mpl::trace_location(category,
                                "cannot set ownership for \'{}\' without id mapping",
                                filename.string());
            return reply_perm_denied(msg);
        }

        if (MP_PLATFORM.chown(filename.string().c_str(),
                              reverse_uid_for(msg->attr->uid, msg->attr->uid),
                              reverse_gid_for(msg->attr->gid, msg->attr->gid)) < 0)
        {
            mpl::trace_location(category, "cannot set ownership for '{}'", filename.string());
            return reply_failure(msg);
        }
    }

    return reply_ok(msg);
}

int mp::SftpServer::handle_stat(sftp_client_message msg, const bool follow)
{
    const auto filename = get_validated_path(msg);
    if (!filename.has_value())
        return reply_perm_denied(msg);

    QFileInfo file_info(*filename);
    if (!file_info.isSymLink() && !MP_FILEOPS.exists(file_info))
    {
        mpl::trace_location(category, "cannot stat \'{}\': no such file", filename->string());
        return sftp_reply_status(msg, SSH_FX_NO_SUCH_FILE, "no such file");
    }

    sftp_attributes_struct attr{};

    if (!follow && file_info.isSymLink() &&
        mp::platform::symlink_attr_from(filename->string().c_str(), &attr) == 0)
    {
        attr.uid = mapped_uid_for(attr.uid);
        attr.gid = mapped_gid_for(attr.gid);
    }
    else
    {
        // Fallback if symlink_attr_from fails
        if (file_info.isSymLink() && follow)
            file_info = QFileInfo(file_info.symLinkTarget());

        attr = attr_from(file_info);
    }

    return sftp_reply_attr(msg, &attr);
}

int mp::SftpServer::handle_symlink(sftp_client_message msg)
{
    // 9pfs implementation - host only stores and retrieves symlink strings
    //  We do not check target (link can point anywhere), openat is sandboxed
    const auto symlink_target = sftp_client_message_get_filename(msg);
    if (symlink_target == nullptr || *symlink_target == '\0')
    {
        mpl::trace(category, "{}: cannot create an empty symlink", __FUNCTION__);
        return reply_perm_denied(msg);
    }

    // The actual path of the link file must be validated
    const auto link_path = get_absolute_path(sftp_client_message_get_data(msg));

    // Hardcode false: We are CREATING/EDITING a link, so we must never follow it!
    if (!validate_path(link_path, false))
    {
        mpl::trace_location(category,
                            "cannot validate path \'{}\' against source \'{}\'",
                            link_path,
                            source_path);
        return reply_perm_denied(msg);
    }

    // Bug: we were checking against the target path, not the link path
    QFileInfo file_info{link_path};
    if (MP_FILEOPS.exists(file_info) && !has_id_mappings_for(file_info))
    {
        mpl::trace_location(category,
                            "cannot access path \'{}\' without id mapping: permission denied",
                            symlink_target);
        return reply_perm_denied(msg);
    }

    if (!MP_PLATFORM.symlink(symlink_target,
                             link_path.string().c_str(),
                             QFileInfo(symlink_target).isDir()))
    {
        mpl::trace_location(category,
                            "failure creating symlink from \'{}\' to \'{}\'",
                            symlink_target,
                            link_path);
        return reply_failure(msg);
    }

    return reply_ok(msg);
}

int mp::SftpServer::handle_write(sftp_client_message msg)
{
    const auto handle = get_handle<NamedFd>(msg);
    if (handle == nullptr)
    {
        mpl::trace_location(category, "bad handle requested");
        return reply_bad_handle(msg, "write");
    }

    const auto& [path, file] = *handle;

    if (MP_FILEOPS.lseek(file, msg->offset, SEEK_SET) == -1)
    {
        mpl::trace_location(category,
                            "cannot seek to position {} in '{}'",
                            msg->offset,
                            path.string());
        return reply_failure(msg);
    }

    auto len = ssh_string_len(msg->data);
    auto data_ptr = ssh_string_get_char(msg->data);

    do
    {
        const auto r = MP_FILEOPS.write(file, data_ptr, len);
        if (r == -1)
        {
            mpl::trace_location(category,
                                "write failed for '{}': {}",
                                path.string(),
                                std::strerror(errno));
            return reply_failure(msg);
        }

        data_ptr += r;
        len -= r;
    } while (len > 0);

    return reply_ok(msg);
}

int mp::SftpServer::handle_extended(sftp_client_message msg)
{
    const auto submessage = sftp_client_message_get_submessage(msg);
    if (submessage == nullptr)
    {
        mpl::trace_location(category, "invalid submesage requested");
        return reply_failure(msg);
    }

    const std::string method(submessage);
    if (method == "hardlink@openssh.com")
    {
        // A hardlink needs the target to exist, and as such needs to exist within the mount folder
        const auto hardlink_target = get_validated_path(msg);
        if (!hardlink_target.has_value())
            return reply_perm_denied(msg);
        const auto hardlink_path = get_absolute_path(sftp_client_message_get_data(msg));

        // We may be creating a hard-link, it could not exist
        if (!validate_path(hardlink_path, false))
        {
            mpl::trace_location(category,
                                "cannot validate path \'{}\' against source \'{}\'",
                                hardlink_path,
                                source_path);
            return reply_perm_denied(msg);
        }

        QFileInfo file_info{*hardlink_target};
        if (!has_id_mappings_for(file_info))
        {
            mpl::trace_location(category,
                                "cannot access path \'{}\' without id mapping: permission denied",
                                hardlink_target);
            return reply_perm_denied(msg);
        }

        if (!MP_PLATFORM.link(hardlink_target->string().c_str(), hardlink_path.string().c_str()))
        {
            mpl::trace_location(category,
                                "failed creating link from \'{}\' to \'{}\'",
                                hardlink_target,
                                hardlink_path);
            return reply_failure(msg);
        }
    }
    else if (method == "posix-rename@openssh.com")
    {
        // To override the default file check in SFTP_EXTENDED
        msg->type = SFTP_RENAME;
        return handle_rename(msg);
    }
    else
    {
        mpl::trace(category, "Unhandled extended method requested: {}", method);
        return reply_unsupported(msg);
    }

    return reply_ok(msg);
}

template <typename T>
T* multipass::SftpServer::get_handle(sftp_client_message msg)
{
    auto* handle = static_cast<SftpHandle*>(sftp_handle(msg->sftp, msg->handle));
    if (!handle)
        return nullptr;

    return std::get_if<T>(handle);
}
