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
#include <multipass/logging/log.h>
#include <multipass/platform.h>
#include <multipass/ssh/ssh_session.h>
#include <multipass/ssh/throw_on_error.h>
#include <multipass/utils.h>

#include <QDir>
#include <QFile>

#include <fcntl.h>

namespace mp = multipass;
namespace mpl = multipass::logging;

namespace
{
constexpr auto category = "sftp server";
using SftpHandleUPtr = std::unique_ptr<ssh_string_struct, void (*)(ssh_string)>;
using namespace std::literals::chrono_literals;

enum Permissions
{
    read_user = 0400,
    write_user = 0200,
    exec_user = 0100,
    read_group = 040,
    write_group = 020,
    exec_group = 010,
    read_other = 04,
    write_other = 02,
    exec_other = 01
};

auto make_sftp_session(ssh_session session, ssh_channel channel)
{
    mp::SftpServer::SftpSessionUptr sftp_server_session{sftp_server_new(session, channel), sftp_free};
    mp::SSH::throw_on_error(sftp_server_session, session, "[sftp] server init failed", sftp_server_init);
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
    return sftp_reply_status(msg, SSH_FX_BAD_MESSAGE, fmt::format("{}: invalid handle", type).c_str());
}

int reply_unsupported(sftp_client_message msg)
{
    return sftp_reply_status(msg, SSH_FX_OP_UNSUPPORTED, "Unsupported message");
}

fmt::memory_buffer& operator<<(fmt::memory_buffer& buf, const char* v)
{
    fmt::format_to(std::back_inserter(buf), v);
    return buf;
}

auto longname_from(const QFileInfo& file_info, const std::string& filename)
{
    fmt::memory_buffer out;
    auto mode = file_info.permissions();

    if (file_info.isSymLink())
        out << "l";
    else if (file_info.isDir())
        out << "d";
    else
        out << "-";

    /* user */
    if (mode & QFileDevice::ReadOwner)
        out << "r";
    else
        out << "-";

    if (mode & QFileDevice::WriteOwner)
        out << "w";
    else
        out << "-";

    if (mode & QFileDevice::ExeOwner)
        out << "x";
    else
        out << "-";

    /*group*/
    if (mode & QFileDevice::ReadGroup)
        out << "r";
    else
        out << "-";

    if (mode & QFileDevice::WriteGroup)
        out << "w";
    else
        out << "-";

    if (mode & QFileDevice::ExeGroup)
        out << "x";
    else
        out << "-";

    /* other */
    if (mode & QFileDevice::ReadOther)
        out << "r";
    else
        out << "-";

    if (mode & QFileDevice::WriteOther)
        out << "w";
    else
        out << "-";

    if (mode & QFileDevice::ExeOther)
        out << "x";
    else
        out << "-";

    fmt::format_to(std::back_inserter(out), " 1 {} {} {}", file_info.ownerId(), file_info.groupId(), file_info.size());

    const auto timestamp = file_info.lastModified().toString("MMM d hh:mm:ss yyyy").toStdString();
    fmt::format_to(std::back_inserter(out), " {} {}", timestamp, filename);

    return out;
}

auto to_unix_permissions(QFile::Permissions perms)
{
    int out = 0;

    if (perms & QFileDevice::ReadOwner)
        out |= Permissions::read_user;
    if (perms & QFileDevice::WriteOwner)
        out |= Permissions::write_user;
    if (perms & QFileDevice::ExeOwner)
        out |= Permissions::exec_user;
    if (perms & QFileDevice::ReadGroup)
        out |= Permissions::read_group;
    if (perms & QFileDevice::WriteGroup)
        out |= Permissions::write_group;
    if (perms & QFileDevice::ExeGroup)
        out |= Permissions::exec_group;
    if (perms & QFileDevice::ReadOther)
        out |= Permissions::read_other;
    if (perms & QFileDevice::WriteOther)
        out |= Permissions::write_other;
    if (perms & QFileDevice::ExeOther)
        out |= Permissions::exec_other;

    return out;
}

auto validate_path(const std::string& source_path, const std::string& current_path)
{
    if (source_path.empty())
        return false;

    return current_path.compare(0, source_path.length(), source_path) == 0;
}

void check_sshfs_status(mp::SSHSession& session, mp::SSHProcess& sshfs_process)
{
    if (sshfs_process.exit_recognized(250ms))
    {
        // This `if` is artificial and should not really be here. However there is a complex arrangement of Sftp and
        // SshfsMount tests depending on this.
        if (sshfs_process.exit_code(250ms) != 0) // TODO remove
            throw std::runtime_error(sshfs_process.read_std_error());
    }
}

auto create_sshfs_process(mp::SSHSession& session, const std::string& sshfs_exec_line, const std::string& source,
                          const std::string& target)
{
    auto sshfs_process = session.exec(fmt::format("sudo {} :{:?} {:?}", sshfs_exec_line, source, target));

    check_sshfs_status(session, sshfs_process);

    return std::make_unique<mp::SSHProcess>(std::move(sshfs_process));
}

int mapped_id_for(const mp::id_mappings& id_maps, const int id, const int default_id)
{
    if (id == mp::no_id_info_available)
        return default_id;

    auto found = std::find_if(id_maps.cbegin(), id_maps.cend(), [id](std::pair<int, int> p) { return id == p.first; });

    return found == id_maps.cend() ? -1 : (found->second == mp::default_id ? default_id : found->second);
}

int reverse_id_for(const mp::id_mappings& id_maps, const int id, const int default_id)
{
    auto found = std::find_if(id_maps.cbegin(), id_maps.cend(), [id](std::pair<int, int> p) { return id == p.second; });
    auto default_found = std::find_if(id_maps.cbegin(), id_maps.cend(), [default_id](std::pair<int, int> p) {
        return default_id == p.second;
    });

    return found == id_maps.cend() ? (default_found == id_maps.cend() ? default_id : default_found->first)
                                   : found->first;
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
      source_path{source},
      target_path{target},
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

sftp_attributes_struct mp::SftpServer::attr_from(const QFileInfo& file_info)
{
    sftp_attributes_struct attr{};

    attr.size = file_info.size();

    attr.uid = mapped_uid_for(file_info.ownerId());
    attr.gid = mapped_gid_for(file_info.groupId());

    attr.permissions = to_unix_permissions(file_info.permissions());
    attr.atime = file_info.lastRead().toUTC().toMSecsSinceEpoch() / 1000;
    attr.mtime = file_info.lastModified().toUTC().toMSecsSinceEpoch() / 1000;
    attr.flags =
        SSH_FILEXFER_ATTR_SIZE | SSH_FILEXFER_ATTR_UIDGID | SSH_FILEXFER_ATTR_PERMISSIONS | SSH_FILEXFER_ATTR_ACMODTIME;

    if (file_info.isSymLink())
        attr.permissions |= SSH_S_IFLNK | 0777;
    else if (file_info.isDir())
        attr.permissions |= SSH_S_IFDIR;
    else if (file_info.isFile())
        attr.permissions |= SSH_S_IFREG;

    return attr;
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

bool mp::SftpServer::has_id_mappings_for(const QFileInfo& file_info)
{
    return has_uid_mapping_for(MP_FILEOPS.ownerId(file_info)) && has_gid_mapping_for(MP_FILEOPS.groupId(file_info));
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
        mpl::log(mpl::Level::trace, category, fmt::format("Unknown message: {}", static_cast<int>(type)));
        ret = reply_unsupported(msg);
    }
    if (ret != 0)
        mpl::log(mpl::Level::error, category, fmt::format("error occurred when replying to client: {}", ret));
}

void mp::SftpServer::run()
{
    using MsgUPtr = std::unique_ptr<sftp_client_message_struct, decltype(sftp_client_message_free)*>;

    while (true)
    {
        MsgUPtr client_msg{sftp_get_client_message(sftp_server_session.get()), sftp_client_message_free};
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
            catch (const mp::ExitlessSSHProcessException&) // should we limit this to SSHProcessExitError?
            {
                status = 1;
            }

            if (status != 0)
            {
                mpl::log(mpl::Level::error, category,
                         "sshfs in the instance appears to have exited unexpectedly.  Trying to recover.");

                std::string mount_path = [this] {
                    auto proc = ssh_session.exec(fmt::format("findmnt --source :{}  -o TARGET -n", source_path));
                    return proc.read_std_output();
                }();

                if (!mount_path.empty())
                {
                    ssh_session.exec(fmt::format("sudo umount {}", mount_path));
                }

                sshfs_process = create_sshfs_process(ssh_session, sshfs_exec_line, source_path, target_path);
                sftp_server_session = make_sftp_session(ssh_session, sshfs_process->release_channel());

                continue;
            }
            else
            {
                break;
            }
        }

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
        mpl::log(mpl::Level::trace, category, fmt::format("{}: bad handle requested", __FUNCTION__));
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
        mpl::log(mpl::Level::trace, category, fmt::format("{}: bad handle requested", __FUNCTION__));
        return reply_bad_handle(msg, "fstat");
    }

    const auto& [path, _] = *handle;

    QFileInfo file_info(path.string().c_str());

    if (file_info.isSymLink())
        file_info = QFileInfo(file_info.symLinkTarget());

    auto attr = attr_from(file_info);
    return sftp_reply_attr(msg, &attr);
}

int mp::SftpServer::handle_mkdir(sftp_client_message msg)
{
    const auto filename = sftp_client_message_get_filename(msg);
    if (!validate_path(source_path, filename))
    {
        mpl::log(mpl::Level::trace,
                 category,
                 fmt::format("{}: cannot validate path '{}' against source '{}'", __FUNCTION__, filename, source_path));
        return reply_perm_denied(msg);
    }

    QDir dir(filename);
    QFileInfo current_dir(filename);
    QFileInfo parent_dir(current_dir.path());

    if (!has_id_mappings_for(parent_dir))
    {
        mpl::log(mpl::Level::trace,
                 category,
                 fmt::format("{}: cannot create path \'{}\' with permissions \'{}:{}\': permission denied",
                             __FUNCTION__,
                             parent_dir.ownerId(),
                             parent_dir.groupId(),
                             filename));
        return reply_perm_denied(msg);
    }

    if (!dir.mkdir(filename))
    {
        mpl::log(mpl::Level::trace, category, fmt::format("{}: mkdir failed for '{}'", __FUNCTION__, filename));
        return reply_failure(msg);
    }

    std::error_code err;
    MP_FILEOPS.permissions(filename, static_cast<fs::perms>(msg->attr->permissions), err);
    if (err)
    {
        mpl::log(mpl::Level::trace,
                 category,
                 fmt::format("{}: set permissions failed for '{}': {}", __FUNCTION__, filename, err.message()));
        return reply_failure(msg);
    }

    int rev_uid = reverse_uid_for(parent_dir.ownerId(), parent_dir.ownerId());
    int rev_gid = reverse_gid_for(parent_dir.groupId(), parent_dir.groupId());

    if (MP_PLATFORM.chown(filename, rev_uid, rev_gid) < 0)
    {
        mpl::log(mpl::Level::trace, category,
                 fmt::format("failed to chown '{}' to owner:{} and group:{}", filename, rev_uid, rev_gid));
        return reply_failure(msg);
    }

    return reply_ok(msg);
}

int mp::SftpServer::handle_rmdir(sftp_client_message msg)
{
    const auto filename = sftp_client_message_get_filename(msg);
    if (!validate_path(source_path, filename))
    {
        mpl::log(mpl::Level::trace,
                 category,
                 fmt::format("{}: cannot validate path '{}' against source '{}'", __FUNCTION__, filename, source_path));
        return reply_perm_denied(msg);
    }

    QFileInfo current_dir(filename);
    if (MP_FILEOPS.exists(current_dir) && !has_id_mappings_for(current_dir))
    {
        mpl::log(
            mpl::Level::trace,
            category,
            fmt::format("{}: cannot access path \'{}\' without id mapping: permission denied", __FUNCTION__, filename));
        return reply_perm_denied(msg);
    }

    std::error_code err;
    if (!MP_FILEOPS.remove(filename, err) && !err)
        err = std::make_error_code(std::errc::no_such_file_or_directory);

    if (err)
    {
        mpl::log(mpl::Level::trace,
                 category,
                 fmt::format("{}: rmdir failed for '{}': {}", __FUNCTION__, filename, err.message()));
        return reply_failure(msg);
    }

    return reply_ok(msg);
}

int mp::SftpServer::handle_open(sftp_client_message msg)
{
    const auto filename = sftp_client_message_get_filename(msg);
    if (!validate_path(source_path, filename))
    {
        mpl::log(mpl::Level::trace,
                 category,
                 fmt::format("{}: cannot validate path '{}' against source '{}'", __FUNCTION__, filename, source_path));
        return reply_perm_denied(msg);
    }

    std::error_code err;
    const auto status = MP_FILEOPS.symlink_status(filename, err);
    if (err && status.type() != fs::file_type::not_found)
    {
        mpl::log(mpl::Level::trace, category, fmt::format("Cannot get status of '{}': {}", filename, err.message()));
        return reply_perm_denied(msg);
    }
    const auto exists = fs::is_symlink(status) || fs::is_regular_file(status);

    QFileInfo file_info(filename);
    QFileInfo current_dir(file_info.path());
    if ((exists && !has_id_mappings_for(file_info)) || (!exists && !has_id_mappings_for(current_dir)))
    {
        mpl::log(
            mpl::Level::trace,
            category,
            fmt::format("{}: cannot access path \'{}\' without id mapping: permission denied", __FUNCTION__, filename));
        return reply_perm_denied(msg);
    }

    int mode = 0;
    const auto flags = sftp_client_message_get_flags(msg);

    if (flags & SSH_FXF_READ)
        mode |= O_RDONLY;

    if (flags & SSH_FXF_WRITE)
        mode |= O_WRONLY;

    if ((flags & SSH_FXF_READ) && (flags & SSH_FXF_WRITE))
    {
        mode &= ~O_RDONLY;
        mode &= ~O_WRONLY;
        mode |= O_RDWR;
    }

    if (flags & SSH_FXF_APPEND)
        mode |= O_APPEND;

    if (flags & SSH_FXF_TRUNC)
        mode |= O_TRUNC;

    if (flags & SSH_FXF_CREAT)
        mode |= O_CREAT;

    if (flags & SSH_FXF_EXCL)
        mode |= O_EXCL;

    auto named_fd = MP_FILEOPS.open_fd(filename, mode, msg->attr ? msg->attr->permissions : 0);
    if (named_fd->fd == -1)
    {
        mpl::log(mpl::Level::trace, category, fmt::format("Cannot open '{}': {}", filename, std::strerror(errno)));
        return reply_failure(msg);
    }

    if (!exists)
    {
        auto new_uid = reverse_uid_for(current_dir.ownerId(), current_dir.ownerId());
        auto new_gid = reverse_gid_for(current_dir.groupId(), current_dir.groupId());

        if (MP_PLATFORM.chown(filename, new_uid, new_gid) < 0)
        {
            mpl::log(mpl::Level::trace, category,
                     fmt::format("failed to chown '{}' to owner:{} and group:{}", filename, new_uid, new_gid));
            return reply_failure(msg);
        }
    }

    SftpHandleUPtr sftp_handle{sftp_handle_alloc(sftp_server_session.get(), named_fd.get()), ssh_string_free};
    if (!sftp_handle)
    {
        mpl::log(mpl::Level::trace, category, "Cannot allocate handle for open()");
        return reply_failure(msg);
    }

    open_file_handles.emplace(named_fd.get(), std::move(named_fd));

    return sftp_reply_handle(msg, sftp_handle.get());
}

int mp::SftpServer::handle_opendir(sftp_client_message msg)
{
    const auto filename = sftp_client_message_get_filename(msg);
    if (!validate_path(source_path, filename))
    {
        mpl::log(mpl::Level::trace,
                 category,
                 fmt::format("{}: cannot validate path '{}' against source '{}'", __FUNCTION__, filename, source_path));
        return reply_perm_denied(msg);
    }

    std::error_code err;
    auto dir_iterator = MP_FILEOPS.dir_iterator(filename, err);

    if (err.value() == int(std::errc::no_such_file_or_directory) || err.value() == int(std::errc::no_such_process))
    {
        mpl::log(mpl::Level::trace, category, fmt::format("Cannot open directory '{}': {}", filename, err.message()));
        return sftp_reply_status(msg, SSH_FX_NO_SUCH_FILE, "no such directory");
    }

    if (err.value() == int(std::errc::permission_denied))
    {
        mpl::log(mpl::Level::trace, category, fmt::format("Cannot read directory '{}': {}", filename, err.message()));
        return reply_perm_denied(msg);
    }

    QFileInfo file_info{filename};
    if (!has_id_mappings_for(file_info))
    {
        mpl::log(
            mpl::Level::trace,
            category,
            fmt::format("{}: cannot access path \'{}\' without id mapping: permission denied", __FUNCTION__, filename));
        return reply_perm_denied(msg);
    }

    SftpHandleUPtr sftp_handle{sftp_handle_alloc(sftp_server_session.get(), dir_iterator.get()), ssh_string_free};
    if (!sftp_handle)
    {
        mpl::log(mpl::Level::trace, category, "Cannot allocate handle for opendir()");
        return reply_failure(msg);
    }

    open_dir_handles.emplace(dir_iterator.get(), std::move(dir_iterator));

    return sftp_reply_handle(msg, sftp_handle.get());
}

int mp::SftpServer::handle_read(sftp_client_message msg)
{
    const auto handle = get_handle<NamedFd>(msg);
    if (handle == nullptr)
    {
        mpl::log(mpl::Level::trace, category, fmt::format("{}: bad handle requested", __FUNCTION__));
        return reply_bad_handle(msg, "read");
    }

    const auto& [path, file] = *handle;

    if (MP_FILEOPS.lseek(file, msg->offset, SEEK_SET) == -1)
    {
        mpl::log(mpl::Level::trace,
                 category,
                 fmt::format("{}: cannot seek to position {} in '{}'", __FUNCTION__, msg->offset, path.string()));
        return reply_failure(msg);
    }

    constexpr auto max_packet_size = 65536u;
    std::array<char, max_packet_size> buffer{};

    if (const auto r = MP_FILEOPS.read(file, buffer.data(), std::min(msg->len, max_packet_size)); r > 0)
        return sftp_reply_data(msg, buffer.data(), r);
    else if (r == 0)
        return sftp_reply_status(msg, SSH_FX_EOF, "End of file");

    mpl::log(mpl::Level::trace,
             category,
             fmt::format("{}: read failed for '{}': {}", __FUNCTION__, path.string(), std::strerror(errno)));
    return sftp_reply_status(msg, SSH_FX_FAILURE, std::strerror(errno));
}

int mp::SftpServer::handle_readdir(sftp_client_message msg)
{
    const auto handle = get_handle<DirIterator>(msg);
    if (handle == nullptr)
    {
        mpl::log(mpl::Level::trace, category, fmt::format("{}: bad handle requested", __FUNCTION__));
        return reply_bad_handle(msg, "readdir");
    }

    auto& dir_iterator = *handle;

    if (!dir_iterator.hasNext())
        return sftp_reply_status(msg, SSH_FX_EOF, nullptr);

    const auto max_num_entries_per_packet = 50ll;
    for (int i = 0; i < max_num_entries_per_packet && dir_iterator.hasNext(); i++)
    {
        const auto& entry = dir_iterator.next();
        QFileInfo file_info{entry.path().string().c_str()};
        sftp_attributes_struct attr{};
        if (entry.is_symlink())
        {
            mp::platform::symlink_attr_from(file_info.absoluteFilePath().toStdString().c_str(), &attr);
            attr.uid = mapped_uid_for(attr.uid);
            attr.gid = mapped_gid_for(attr.gid);
        }
        else
        {
            attr = attr_from(file_info);
        }
        const auto longname = longname_from(file_info, entry.path().string());
        sftp_reply_names_add(msg, entry.path().filename().string().c_str(), longname.data(), &attr);
    }

    return sftp_reply_names(msg);
}

int mp::SftpServer::handle_readlink(sftp_client_message msg)
{
    auto filename = sftp_client_message_get_filename(msg);
    if (!validate_path(source_path, filename))
    {
        mpl::log(
            mpl::Level::trace, category,
            fmt::format("{}: cannot validate path \'{}\' against source \'{}\'", __FUNCTION__, filename, source_path));
        return reply_perm_denied(msg);
    }

    auto link = QFile::symLinkTarget(filename);
    if (link.isEmpty())
    {
        mpl::log(mpl::Level::trace, category, fmt::format("{}: invalid link for \'{}\'", __FUNCTION__, filename));
        return sftp_reply_status(msg, SSH_FX_NO_SUCH_FILE, "invalid link");
    }

    QFileInfo file_info{filename};
    if (!has_id_mappings_for(file_info))
    {
        mpl::log(
            mpl::Level::trace,
            category,
            fmt::format("{}: cannot access path \'{}\' without id mapping: permission denied", __FUNCTION__, filename));
        return reply_perm_denied(msg);
    }

    sftp_attributes_struct attr{};
    sftp_reply_names_add(msg, link.toStdString().c_str(), link.toStdString().c_str(), &attr);
    return sftp_reply_names(msg);
}

int mp::SftpServer::handle_realpath(sftp_client_message msg)
{
    auto filename = sftp_client_message_get_filename(msg);
    if (!validate_path(source_path, filename))
    {
        mpl::log(
            mpl::Level::trace, category,
            fmt::format("{}: cannot validate path \'{}\' against source \'{}\'", __FUNCTION__, filename, source_path));
        return reply_perm_denied(msg);
    }

    QFileInfo file_info{filename};
    if (!has_id_mappings_for(file_info))
    {
        mpl::log(
            mpl::Level::trace,
            category,
            fmt::format("{}: cannot access path \'{}\' without id mapping: permission denied", __FUNCTION__, filename));
        return reply_perm_denied(msg);
    }

    auto realpath = QFileInfo(filename).absoluteFilePath();
    return sftp_reply_name(msg, realpath.toStdString().c_str(), nullptr);
}

int mp::SftpServer::handle_remove(sftp_client_message msg)
{
    const auto filename = sftp_client_message_get_filename(msg);
    if (!validate_path(source_path, filename))
    {
        mpl::log(mpl::Level::trace,
                 category,
                 fmt::format("{}: cannot validate path '{}' against source '{}'", __FUNCTION__, filename, source_path));
        return reply_perm_denied(msg);
    }

    QFileInfo file_info{filename};
    if (MP_FILEOPS.exists(file_info) && !has_id_mappings_for(file_info))
    {
        mpl::log(
            mpl::Level::trace,
            category,
            fmt::format("{}: cannot access path \'{}\' without id mapping: permission denied", __FUNCTION__, filename));
        return reply_perm_denied(msg);
    }

    std::error_code err;
    if (!MP_FILEOPS.remove(filename, err) && !err)
        err = std::make_error_code(std::errc::no_such_file_or_directory);

    if (err)
    {
        mpl::log(mpl::Level::trace,
                 category,
                 fmt::format("{}: cannot remove '{}': {}", __FUNCTION__, filename, err.message()));
        return reply_failure(msg);
    }

    return reply_ok(msg);
}

int mp::SftpServer::handle_rename(sftp_client_message msg)
{
    const auto source = sftp_client_message_get_filename(msg);
    if (!validate_path(source_path, source))
    {
        mpl::log(
            mpl::Level::trace, category,
            fmt::format("{}: cannot validate path \'{}\' against source \'{}\'", __FUNCTION__, source, source_path));
        return reply_perm_denied(msg);
    }

    QFileInfo source_info{source};
    if (!source_info.isSymLink() && !MP_FILEOPS.exists(source_info))
    {
        mpl::log(mpl::Level::trace, category,
                 fmt::format("{}: cannot rename \'{}\': no such file", __FUNCTION__, source));
        return sftp_reply_status(msg, SSH_FX_NO_SUCH_FILE, "no such file");
    }

    if (!has_id_mappings_for(source_info))
    {
        mpl::log(
            mpl::Level::trace,
            category,
            fmt::format("{}: cannot access path \'{}\' without id mapping: permission denied", __FUNCTION__, source));
        return reply_perm_denied(msg);
    }

    const auto target = sftp_client_message_get_data(msg);
    if (!validate_path(source_path, target))
    {
        mpl::log(mpl::Level::trace, category,
                 fmt::format("{}: cannot validate target path \'{}\' against source \'{}\'", __FUNCTION__, target,
                             source_path));
        return reply_perm_denied(msg);
    }

    QFileInfo target_info{target};
    if (MP_FILEOPS.exists(target_info) && !has_id_mappings_for(target_info))
    {
        mpl::log(
            mpl::Level::trace,
            category,
            fmt::format("{}: cannot access path \'{}\' without id mapping: permission denied", __FUNCTION__, target));
        return reply_perm_denied(msg);
    }

    QFile target_file{target};
    if (MP_FILEOPS.exists(target_info))
    {
        if (!MP_FILEOPS.remove(target_file))
        {
            mpl::log(mpl::Level::trace, category,
                     fmt::format("{}: cannot remove \'{}\' for renaming", __FUNCTION__, target));
            return reply_failure(msg);
        }
    }

    QFile source_file{source};
    if (!MP_FILEOPS.rename(source_file, target))
    {
        mpl::log(mpl::Level::trace, category,
                 fmt::format("{}: failed renaming \'{}\' to \'{}\'", __FUNCTION__, source, target));
        return reply_failure(msg);
    }

    return reply_ok(msg);
}

int mp::SftpServer::handle_setstat(sftp_client_message msg)
{
    QString filename;

    if (sftp_client_message_get_type(msg) == SFTP_FSETSTAT)
    {
        const auto handle = get_handle<NamedFd>(msg);
        if (handle == nullptr)
        {
            mpl::log(mpl::Level::trace, category, fmt::format("{}: bad handle requested", __FUNCTION__));
            return reply_bad_handle(msg, "setstat");
        }

        const auto& [path, _] = *handle;
        filename = path.string().c_str();
    }
    else
    {
        filename = sftp_client_message_get_filename(msg);
        if (!validate_path(source_path, filename.toStdString()))
        {
            mpl::log(
                mpl::Level::trace,
                category,
                fmt::format("{}: cannot validate path '{}' against source '{}'", __FUNCTION__, filename, source_path));
            return reply_perm_denied(msg);
        }

        QFileInfo file_info{filename};
        if (!file_info.isSymLink() && !MP_FILEOPS.exists(file_info))
        {
            mpl::log(mpl::Level::trace,
                     category,
                     fmt::format("{}: cannot setstat '{}': no such file", __FUNCTION__, filename));
            return sftp_reply_status(msg, SSH_FX_NO_SUCH_FILE, "no such file");
        }
    }

    QFile file{filename};
    QFileInfo file_info{filename};
    if (!has_id_mappings_for(file_info))
    {
        mpl::log(
            mpl::Level::trace,
            category,
            fmt::format("{}: cannot access path \'{}\' without id mapping: permission denied", __FUNCTION__, filename));
        return reply_perm_denied(msg);
    }

    if (msg->attr->flags & SSH_FILEXFER_ATTR_SIZE)
    {
        if (!MP_FILEOPS.resize(file, msg->attr->size))
        {
            mpl::log(mpl::Level::trace, category, fmt::format("{}: cannot resize '{}'", __FUNCTION__, filename));
            return reply_failure(msg);
        }
    }

    if (msg->attr->flags & SSH_FILEXFER_ATTR_PERMISSIONS)
    {
        std::error_code err;
        MP_FILEOPS.permissions(file.fileName().toStdString(), static_cast<fs::perms>(msg->attr->permissions), err);
        if (err)
        {
            mpl::log(mpl::Level::trace,
                     category,
                     fmt::format("{}: set permissions failed for '{}': {}", __FUNCTION__, filename, err.message()));
            return reply_failure(msg);
        }
    }

    if (msg->attr->flags & SSH_FILEXFER_ATTR_ACMODTIME)
    {
        if (MP_PLATFORM.utime(filename.toStdString().c_str(), msg->attr->atime, msg->attr->mtime) < 0)
        {
            mpl::log(mpl::Level::trace,
                     category,
                     fmt::format("{}: cannot set modification date for '{}'", __FUNCTION__, filename));
            return reply_failure(msg);
        }
    }

    if (msg->attr->flags & SSH_FILEXFER_ATTR_UIDGID)
    {
        if (!has_reverse_uid_mapping_for(msg->attr->uid) && !has_reverse_gid_mapping_for(msg->attr->gid))
        {
            mpl::log(mpl::Level::trace,
                     category,
                     fmt::format("{}: cannot set ownership for \'{}\' without id mapping", __FUNCTION__, filename));
            return reply_perm_denied(msg);
        }

        if (MP_PLATFORM.chown(filename.toStdString().c_str(),
                              reverse_uid_for(msg->attr->uid, msg->attr->uid),
                              reverse_gid_for(msg->attr->gid, msg->attr->gid)) < 0)
        {
            mpl::log(mpl::Level::trace,
                     category,
                     fmt::format("{}: cannot set ownership for '{}'", __FUNCTION__, filename));
            return reply_failure(msg);
        }
    }

    return reply_ok(msg);
}

int mp::SftpServer::handle_stat(sftp_client_message msg, const bool follow)
{
    auto filename = sftp_client_message_get_filename(msg);
    if (!validate_path(source_path, filename))
    {
        mpl::log(
            mpl::Level::trace, category,
            fmt::format("{}: cannot validate path \'{}\' against source \'{}\'", __FUNCTION__, filename, source_path));
        return reply_perm_denied(msg);
    }

    QFileInfo file_info(filename);
    if (!file_info.isSymLink() && !MP_FILEOPS.exists(file_info))
    {
        mpl::log(mpl::Level::trace,
                 category,
                 fmt::format("{}: cannot stat \'{}\': no such file", __FUNCTION__, filename));
        return sftp_reply_status(msg, SSH_FX_NO_SUCH_FILE, "no such file");
    }

    sftp_attributes_struct attr{};

    if (!follow && file_info.isSymLink())
    {
        mp::platform::symlink_attr_from(filename, &attr);
        attr.uid = mapped_uid_for(attr.uid);
        attr.gid = mapped_gid_for(attr.gid);
    }
    else
    {
        if (file_info.isSymLink())
            file_info = QFileInfo(file_info.symLinkTarget());

        attr = attr_from(file_info);
    }

    return sftp_reply_attr(msg, &attr);
}

int mp::SftpServer::handle_symlink(sftp_client_message msg)
{
    const auto old_name = sftp_client_message_get_filename(msg);
    const auto new_name = sftp_client_message_get_data(msg);

    if (!validate_path(source_path, new_name))
    {
        mpl::log(
            mpl::Level::trace, category,
            fmt::format("{}: cannot validate path \'{}\' against source \'{}\'", __FUNCTION__, new_name, source_path));
        return reply_perm_denied(msg);
    }

    QFileInfo file_info{old_name};
    if (MP_FILEOPS.exists(file_info) && !has_id_mappings_for(file_info))
    {
        mpl::log(
            mpl::Level::trace,
            category,
            fmt::format("{}: cannot access path \'{}\' without id mapping: permission denied", __FUNCTION__, old_name));
        return reply_perm_denied(msg);
    }

    if (!MP_PLATFORM.symlink(old_name, new_name, QFileInfo(old_name).isDir()))
    {
        mpl::log(mpl::Level::trace, category,
                 fmt::format("{}: failure creating symlink from \'{}\' to \'{}\'", __FUNCTION__, old_name, new_name));
        return reply_failure(msg);
    }

    return reply_ok(msg);
}

int mp::SftpServer::handle_write(sftp_client_message msg)
{
    const auto handle = get_handle<NamedFd>(msg);
    if (handle == nullptr)
    {
        mpl::log(mpl::Level::trace, category, fmt::format("{}: bad handle requested", __FUNCTION__));
        return reply_bad_handle(msg, "write");
    }

    const auto& [path, file] = *handle;

    if (MP_FILEOPS.lseek(file, msg->offset, SEEK_SET) == -1)
    {
        mpl::log(mpl::Level::trace,
                 category,
                 fmt::format("{}: cannot seek to position {} in '{}'", __FUNCTION__, msg->offset, path.string()));
        return reply_failure(msg);
    }

    auto len = ssh_string_len(msg->data);
    auto data_ptr = ssh_string_get_char(msg->data);

    do
    {
        const auto r = MP_FILEOPS.write(file, data_ptr, len);
        if (r == -1)
        {
            mpl::log(mpl::Level::trace,
                     category,
                     fmt::format("{}: write failed for '{}': {}", __FUNCTION__, path.string(), std::strerror(errno)));
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
        mpl::log(mpl::Level::trace, category, fmt::format("{}: invalid submesage requested", __FUNCTION__));
        return reply_failure(msg);
    }

    const std::string method(submessage);
    if (method == "hardlink@openssh.com")
    {
        const auto old_name = sftp_client_message_get_filename(msg);
        const auto new_name = sftp_client_message_get_data(msg);

        if (!validate_path(source_path, new_name))
        {
            mpl::log(mpl::Level::trace, category,
                     fmt::format("{}: cannot validate path \'{}\' against source \'{}\'", __FUNCTION__, new_name,
                                 source_path));
            return reply_perm_denied(msg);
        }

        QFileInfo file_info{old_name};
        if (!has_id_mappings_for(file_info))
        {
            mpl::log(mpl::Level::trace,
                     category,
                     fmt::format("{}: cannot access path \'{}\' without id mapping: permission denied",
                                 __FUNCTION__,
                                 old_name));
            return reply_perm_denied(msg);
        }

        if (!MP_PLATFORM.link(old_name, new_name))
        {
            mpl::log(mpl::Level::trace, category,
                     fmt::format("{}: failed creating link from \'{}\' to \'{}\'", __FUNCTION__, old_name, new_name));
            return reply_failure(msg);
        }
    }
    else if (method == "posix-rename@openssh.com")
    {
        return handle_rename(msg);
    }
    else
    {
        mpl::log(mpl::Level::trace, category, fmt::format("Unhandled extended method requested: {}", method));
        return reply_unsupported(msg);
    }

    return reply_ok(msg);
}

template <typename T>
T* multipass::SftpServer::get_handle(sftp_client_message msg)
{
    return static_cast<T*>(sftp_handle(msg->sftp, msg->handle));
}
