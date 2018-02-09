/*
 * Copyright (C) 2017-2018 Canonical, Ltd.
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

#include <multipass/sshfs_mount/sftp_server.h>

#include <multipass/platform.h>
#include <multipass/ssh/ssh_session.h>
#include <multipass/ssh/throw_on_error.h>
#include <multipass/utime.h>

#include <fmt/format.h>

#include <QDateTime>
#include <QDir>
#include <QFile>

namespace mp = multipass;

namespace
{
using SftpHandleUPtr = std::unique_ptr<ssh_string_struct, void (*)(ssh_string)>;

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

auto make_ssh_session()
{
    mp::SftpServer::SSHSessionUptr session{ssh_new(), ssh_free};
    if (session == nullptr)
        throw std::runtime_error("unable to obtain ssh session for sftp server");
    return session;
}

auto make_sftp_session(ssh_session session, ssh_channel channel)
{
    mp::SftpServer::SftpSessionUptr sftp_server_session{sftp_server_new(session, channel), sftp_free};
    mp::SSH::throw_on_error(sftp_server_init, sftp_server_session);
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

auto longname_from(const QFileInfo& file_info, const std::string& filename)
{
    fmt::MemoryWriter out;
    auto mode = file_info.permissions();

    if (file_info.isSymLink())
        out << "l";
    else if (file_info.isDir())
        out << "d";
    else
        out << "-";

    /* user */
    if (mode & QFileDevice::ReadUser)
        out << "r";
    else
        out << "-";

    if (mode & QFileDevice::WriteUser)
        out << "w";
    else
        out << "-";

    if (mode & QFileDevice::ExeUser)
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

    out.write(" 1 {} {} {}", file_info.ownerId(), file_info.groupId(), file_info.size());

    const auto timestamp = file_info.lastModified().toString("MMM d hh:mm:ss yyyy").toStdString();
    out.write(" {} {}", timestamp, filename);

    return out;
}

auto to_qt_permissions(int perms)
{
    QFile::Permissions out;

    if (perms & Permissions::read_user)
        out |= QFileDevice::ReadUser;
    if (perms & Permissions::write_user)
        out |= QFileDevice::WriteUser;
    if (perms & Permissions::exec_user)
        out |= QFileDevice::ExeUser;
    if (perms & Permissions::read_group)
        out |= QFileDevice::ReadGroup;
    if (perms & Permissions::write_group)
        out |= QFileDevice::WriteGroup;
    if (perms & Permissions::exec_group)
        out |= QFileDevice::ExeGroup;
    if (perms & Permissions::read_other)
        out |= QFileDevice::ReadOther;
    if (perms & Permissions::write_other)
        out |= QFileDevice::WriteOther;
    if (perms & Permissions::exec_other)
        out |= QFileDevice::ExeOther;

    return out;
}

auto to_unix_permissions(QFile::Permissions perms)
{
    int out = 0;

    if (perms & QFileDevice::ReadUser)
        out |= Permissions::read_user;
    if (perms & QFileDevice::WriteUser)
        out |= Permissions::write_user;
    if (perms & QFileDevice::ExeUser)
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

template <typename T>
auto handle_from(sftp_client_message msg, const std::unordered_map<void*, std::unique_ptr<T>>& handles) -> T*
{
    const auto id = sftp_handle(msg->sftp, msg->handle);
    auto entry = handles.find(id);
    if (entry != handles.end())
        return entry->second.get();
    return nullptr;
}
} // namespace

mp::SftpServer::SftpServer(SSHSession&& ssh_session, SSHProcess&& sshfs_proc,
                           const std::unordered_map<int, int>& gid_map, const std::unordered_map<int, int>& uid_map,
                           int default_uid, int default_gid, std::ostream& cout)
    : ssh_session{std::move(ssh_session)},
      sftp_ssh_session{make_ssh_session()},
      sftp_server_session{make_sftp_session(sftp_ssh_session.get(), sshfs_proc.release_channel())},
      gid_map{gid_map},
      uid_map{uid_map},
      default_uid{default_uid},
      default_gid{default_gid},
      cout{cout}
{
}

sftp_attributes_struct mp::SftpServer::attr_from(const QFileInfo& file_info)
{
    sftp_attributes_struct attr{};

    attr.size = file_info.size();

    const auto no_id_info_available = static_cast<uint>(-2);
    if (file_info.ownerId() == no_id_info_available)
    {
        attr.uid = default_uid;
    }
    else
    {
        auto map = uid_map.find(file_info.ownerId());
        if (map != uid_map.end())
        {
            attr.uid = map->second;
        }
        else
        {
            attr.uid = file_info.ownerId();
        }
    }

    if (file_info.groupId() == no_id_info_available)
    {
        attr.gid = default_gid;
    }
    else
    {
        auto map = gid_map.find(file_info.groupId());
        if (map != gid_map.end())
        {
            attr.gid = map->second;
        }
        else
        {
            attr.gid = file_info.groupId();
        }
    }

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
    default:
        cout << "[sftp server] Unknown message: " << static_cast<int>(type) << "\n";
        ret = sftp_reply_status(msg, SSH_FX_OP_UNSUPPORTED, "Unsupported message");
    }
    if (ret != 0)
        cout << "[sftp server] error occurred when replying to client\n";
}

void mp::SftpServer::run()
{
    using MsgUPtr = std::unique_ptr<sftp_client_message_struct, decltype(sftp_client_message_free)*>;

    while (true)
    {
        MsgUPtr client_msg{sftp_get_client_message(sftp_server_session.get()), sftp_client_message_free};
        auto msg = client_msg.get();
        if (msg == nullptr)
            break;

        process_message(msg);
    }
}

void mp::SftpServer::stop()
{
    ssh_session.force_shutdown();
}

int mp::SftpServer::handle_close(sftp_client_message msg)
{
    const auto id = sftp_handle(sftp_server_session.get(), msg->handle);

    auto erased = open_file_handles.erase(id);
    erased += open_dir_handles.erase(id);
    if (erased == 0)
        return reply_bad_handle(msg, "close");

    sftp_handle_remove(sftp_server_session.get(), id);
    return reply_ok(msg);
}

int mp::SftpServer::handle_fstat(sftp_client_message msg)
{
    auto file = handle_from(msg, open_file_handles);
    if (file == nullptr)
        return reply_bad_handle(msg, "fstat");

    QFileInfo file_info(*file);

    if (file_info.isSymLink())
        file_info = QFileInfo(file_info.symLinkTarget());

    auto attr = attr_from(file_info);
    return sftp_reply_attr(msg, &attr);
}

int mp::SftpServer::handle_mkdir(sftp_client_message msg)
{
    const auto filename = sftp_client_message_get_filename(msg);
    QDir dir(filename);
    if (!dir.mkdir(filename))
        return reply_failure(msg);

    if (!QFile::setPermissions(filename, to_qt_permissions(msg->attr->permissions)))
        return reply_failure(msg);

    QFileInfo current_dir(filename);
    QFileInfo parent_dir(current_dir.path());
    auto ret = mp::platform::chown(filename, parent_dir.ownerId(), parent_dir.groupId());
    if (ret < 0)
    {
        cout << fmt::format("[sftp server] failed to chown '{}' to owner:{} and group:{}\n", filename,
                            parent_dir.ownerId(), parent_dir.groupId());
        return reply_failure(msg);
    }
    return reply_ok(msg);
}

int mp::SftpServer::handle_rmdir(sftp_client_message msg)
{
    const auto filename = sftp_client_message_get_filename(msg);
    QDir dir(filename);
    if (!dir.rmdir(filename))
        return reply_failure(msg);

    return reply_ok(msg);
}

int mp::SftpServer::handle_open(sftp_client_message msg)
{
    QIODevice::OpenMode mode = 0;
    const auto flags = sftp_client_message_get_flags(msg);
    if (flags & SSH_FXF_READ)
        mode |= QIODevice::ReadOnly;

    if (flags & SSH_FXF_WRITE)
    {
        mode |= QIODevice::WriteOnly;

        // This is needed to workaround an issue where sshfs does not pass through
        // O_APPEND.  This is fixed in sshfs v. 3.2.
        // Note: This goes against the default behavior of open().
        if (flags == SSH_FXF_WRITE)
        {
            mode |= QIODevice::Append;
            cout << "[sftp server] Adding sshfs O_APPEND workaround.\n";
        }
    }

    if (flags & SSH_FXF_APPEND)
        mode |= QIODevice::Append;

    if (flags & SSH_FXF_TRUNC)
        mode |= QIODevice::Truncate;

    const auto filename = sftp_client_message_get_filename(msg);
    auto file = std::make_unique<QFile>(filename);

    auto exists = file->exists();

    if (!file->open(mode))
        return reply_failure(msg);

    if (!exists)
    {
        if (!file->setPermissions(to_qt_permissions(msg->attr->permissions)))
            return reply_failure(msg);

        QFileInfo current_file(filename);
        QFileInfo current_dir(current_file.path());
        auto ret = mp::platform::chown(filename, current_dir.ownerId(), current_dir.groupId());
        if (ret < 0)
        {
            fmt::format("[sftp server] failed to chown '{}' to owner:{} and group:{}\n", filename,
                        current_dir.ownerId(), current_dir.groupId());
            return reply_failure(msg);
        }
    }

    SftpHandleUPtr sftp_handle{sftp_handle_alloc(sftp_server_session.get(), file.get()), ssh_string_free};
    open_file_handles.emplace(file.get(), std::move(file));

    return sftp_reply_handle(msg, sftp_handle.get());
}

int mp::SftpServer::handle_opendir(sftp_client_message msg)
{
    QDir dir(sftp_client_message_get_filename(msg));
    if (!dir.exists())
        return sftp_reply_status(msg, SSH_FX_NO_SUCH_FILE, "no such directory");

    if (!dir.isReadable())
        return reply_perm_denied(msg);

    auto entry_list =
        std::make_unique<QFileInfoList>(dir.entryInfoList(QDir::AllEntries | QDir::System | QDir::Hidden));

    SftpHandleUPtr sftp_handle{sftp_handle_alloc(sftp_server_session.get(), entry_list.get()), ssh_string_free};
    open_dir_handles.emplace(entry_list.get(), std::move(entry_list));

    return sftp_reply_handle(msg, sftp_handle.get());
}

int mp::SftpServer::handle_read(sftp_client_message msg)
{
    auto file = handle_from(msg, open_file_handles);
    if (file == nullptr)
        return reply_bad_handle(msg, "read");

    const auto max_packet_size = 65536u;
    const auto len = std::min(msg->len, max_packet_size);

    std::vector<char> data;
    data.reserve(len);

    file->seek(msg->offset);
    auto r = file->read(data.data(), len);
    if (r < 0)
        return sftp_reply_status(msg, SSH_FX_FAILURE, file->errorString().toStdString().c_str());
    else if (r == 0)
        return sftp_reply_status(msg, SSH_FX_EOF, "End of file");

    return sftp_reply_data(msg, data.data(), r);
}

int mp::SftpServer::handle_readdir(sftp_client_message msg)
{
    auto dir_entries = handle_from(msg, open_dir_handles);
    if (dir_entries == nullptr)
        return reply_bad_handle(msg, "readdir");

    if (dir_entries->isEmpty())
        return sftp_reply_status(msg, SSH_FX_EOF, nullptr);

    const auto max_num_entries_per_packet = 50;
    const auto num_entries = std::min(dir_entries->size(), max_num_entries_per_packet);

    for (int i = 0; i < num_entries; i++)
    {
        auto entry = dir_entries->takeFirst();
        auto attr = attr_from(entry);
        const auto filename = entry.fileName().toStdString();
        const auto longname = longname_from(entry, filename);
        sftp_reply_names_add(msg, filename.c_str(), longname.c_str(), &attr);
    }

    return sftp_reply_names(msg);
}

int mp::SftpServer::handle_readlink(sftp_client_message msg)
{
    auto link = QFile::symLinkTarget(sftp_client_message_get_filename(msg));
    if (link.isEmpty())
        return sftp_reply_status(msg, SSH_FX_NO_SUCH_FILE, "invalid link");

    sftp_attributes_struct attr{};
    sftp_reply_names_add(msg, link.toStdString().c_str(), link.toStdString().c_str(), &attr);
    return sftp_reply_names(msg);
}

int mp::SftpServer::handle_realpath(sftp_client_message msg)
{
    auto realpath = QFileInfo(sftp_client_message_get_filename(msg)).absoluteFilePath();
    return sftp_reply_name(msg, realpath.toStdString().c_str(), nullptr);
}

int mp::SftpServer::handle_remove(sftp_client_message msg)
{
    if (!QFile::remove(sftp_client_message_get_filename(msg)))
        return reply_failure(msg);
    return reply_ok(msg);
}

int mp::SftpServer::handle_rename(sftp_client_message msg)
{
    const auto target = sftp_client_message_get_data(msg);
    const auto source = sftp_client_message_get_filename(msg);
    if (QFile::exists(target))
    {
        if (!QFile::remove(target))
            return reply_failure(msg);
    }

    if (!QFile::rename(source, target))
        return reply_failure(msg);

    return reply_ok(msg);
}

int mp::SftpServer::handle_setstat(sftp_client_message msg)
{
    QString filename;

    if (sftp_client_message_get_type(msg) == SFTP_FSETSTAT)
    {
        auto handle = handle_from(msg, open_file_handles);
        if (handle == nullptr)
            return reply_bad_handle(msg, "setstat");
        filename = handle->fileName();
    }
    else
    {
        filename = sftp_client_message_get_filename(msg);
    }

    if (msg->attr->flags & SSH_FILEXFER_ATTR_SIZE)
    {
        if (!QFile::resize(filename, msg->attr->size))
            return reply_failure(msg);
    }

    if (msg->attr->flags & SSH_FILEXFER_ATTR_PERMISSIONS)
    {
        if (!QFile::setPermissions(filename, to_qt_permissions(msg->attr->permissions)))
            return reply_failure(msg);
    }

    if (msg->attr->flags & SSH_FILEXFER_ATTR_ACMODTIME)
    {
        struct utimbuf timebuf;

        timebuf.actime = msg->attr->atime;
        timebuf.modtime = msg->attr->mtime;

        if (utime(filename.toStdString().c_str(), &timebuf) < 0)
            return reply_failure(msg);
    }

    if (msg->attr->flags & SSH_FILEXFER_ATTR_UIDGID)
    {
        if (mp::platform::chown(filename.toStdString().c_str(), msg->attr->uid, msg->attr->gid) < 0)
            return reply_failure(msg);
    }

    return reply_ok(msg);
}

int mp::SftpServer::handle_stat(sftp_client_message msg, const bool follow)
{
    QFileInfo file_info(sftp_client_message_get_filename(msg));
    if (!file_info.exists())
        return sftp_reply_status(msg, SSH_FX_NO_SUCH_FILE, "no such file");

    if (follow && file_info.isSymLink())
        file_info = QFileInfo(file_info.symLinkTarget());

    auto attr = attr_from(file_info);
    return sftp_reply_attr(msg, &attr);
}

int mp::SftpServer::handle_symlink(sftp_client_message msg)
{
    const auto old_name = sftp_client_message_get_filename(msg);
    const auto new_name = sftp_client_message_get_data(msg);
    if (!QFile::link(old_name, new_name))
        return reply_failure(msg);

    return reply_ok(msg);
}

int mp::SftpServer::handle_write(sftp_client_message msg)
{
    auto file = handle_from(msg, open_file_handles);
    if (file == nullptr)
        return reply_bad_handle(msg, "write");

    auto len = ssh_string_len(msg->data);
    auto data_ptr = ssh_string_get_char(msg->data);
    if (!file->seek(msg->offset))
        return reply_failure(msg);

    do
    {
        auto r = file->write(data_ptr, len);
        if (r < 0)
            return reply_failure(msg);

        file->flush();

        data_ptr += r;
        len -= r;
    } while (len > 0);

    return reply_ok(msg);
}
