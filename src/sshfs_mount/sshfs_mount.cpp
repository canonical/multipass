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

#include <multipass/sshfs_mount/sshfs_mount.h>

#include <multipass/exceptions/sshfs_missing_error.h>
#include <multipass/ssh/throw_on_error.h>
#include <multipass/utils.h>

#include <libssh/sftp.h>

#include <fmt/format.h>
#include <utime.h>

#include <QDateTime>
#include <QDir>
#include <QFile>

namespace mp = multipass;

namespace
{
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

int reply_status(sftp_client_message& msg, const int error = 0)
{
    switch (error)
    {
    case EACCES:
        return sftp_reply_status(msg, SSH_FX_PERMISSION_DENIED, "permission denied");
    case ENOENT:
        return sftp_reply_status(msg, SSH_FX_NO_SUCH_FILE, "no such file or directory");
    case ENOTDIR:
        return sftp_reply_status(msg, SSH_FX_FAILURE, "not a directory");
    default:
        return sftp_reply_status(msg, SSH_FX_FAILURE, nullptr);
    }
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
    {
        out << "x";
    }
    else
    {
        out << "-";
    }

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

template <typename Callable>
auto run_cmd(mp::SSHSession& session, std::string&& cmd, Callable&& error_handler)
{
    auto ssh_process = session.exec(cmd);
    if (ssh_process.exit_code() != 0)
        error_handler(ssh_process);
    return ssh_process.read_std_output();
}

auto run_cmd(mp::SSHSession& session, std::string&& cmd)
{
    auto error_handler = [](mp::SSHProcess& proc) { throw std::runtime_error(proc.read_std_error()); };
    return run_cmd(session, std::forward<std::string>(cmd), error_handler);
}

void check_sshfs_is_running(mp::SSHSession& session, mp::SSHProcess& sshfs_process, const std::string& source,
                            const std::string& target)
{
    using namespace std::literals::chrono_literals;

    // Make sure sshfs actually runs
    std::this_thread::sleep_for(250ms);
    auto error_handler = [&sshfs_process](mp::SSHProcess&) {
        throw std::runtime_error(sshfs_process.read_std_error());
    };
    run_cmd(session, fmt::format("pgrep -fx \"sshfs.*{}.*{}\"", source, target), error_handler);
}

void check_sshfs_exists(mp::SSHSession& session)
{
    auto error_handler = [](mp::SSHProcess&) { throw mp::SSHFSMissingError(); };
    run_cmd(session, "which sshfs", error_handler);
}

void make_target_dir(mp::SSHSession& session, const std::string& target)
{
    run_cmd(session, fmt::format("sudo mkdir -p \"{}\"", target));
}

void set_owner_for(mp::SSHSession& session, const std::string& target)
{
    auto vm_user = run_cmd(session, "id -nu");
    auto vm_group = run_cmd(session, "id -ng");
    mp::utils::trim_end(vm_user);
    mp::utils::trim_end(vm_group);

    run_cmd(session, fmt::format("sudo chown {}:{} \"{}\"", vm_user, vm_group, target));
}

auto create_sshfs_process(mp::SSHSession& session, const std::string& source, const std::string& target)
{
    check_sshfs_exists(session);
    make_target_dir(session, target);
    set_owner_for(session, target);

    auto sshfs_process = session.exec(fmt::format(
        "sudo sshfs -o slave -o nonempty -o transform_symlinks -o allow_other :\"{}\" \"{}\"", source, target));

    check_sshfs_is_running(session, sshfs_process, source, target);

    return sshfs_process;
}

class SftpServer
{
public:
    SftpServer(const sftp_session& sftp_server_session, const std::unordered_map<int, int>& gid_map,
               const std::unordered_map<int, int>& uid_map, int default_uid, int default_gid, std::ostream& cout)
        : sftp_server{sftp_server_session},
          gid_map{gid_map},
          uid_map{uid_map},
          default_uid{default_uid},
          default_gid{default_gid},
          cout{cout}
    {
    }

    void process_sftp()
    {
        using MsgUPtr = std::unique_ptr<sftp_client_message_struct, decltype(sftp_client_message_free)*>;

        while (true)
        {
            MsgUPtr client_msg{sftp_get_client_message(sftp_server), sftp_client_message_free};
            auto msg = client_msg.get();
            if (msg == nullptr)
                break;

            const auto type = sftp_client_message_get_type(msg);
            switch (type)
            {
            case SFTP_REALPATH:
                handle_realpath(msg);
                break;
            case SFTP_OPENDIR:
                handle_opendir(msg);
                break;
            case SFTP_MKDIR:
                handle_mkdir(msg);
                break;
            case SFTP_RMDIR:
                handle_rmdir(msg);
                break;
            case SFTP_LSTAT:
            case SFTP_STAT:
                handle_stat(msg, type == SFTP_STAT);
                break;
            case SFTP_FSTAT:
                handle_fstat(msg);
                break;
            case SFTP_READDIR:
                handle_readdir(msg);
                break;
            case SFTP_CLOSE:
                handle_close(msg);
                break;
            case SFTP_OPEN:
                handle_open(msg);
                break;
            case SFTP_READ:
                handle_read(msg);
                break;
            case SFTP_WRITE:
                handle_write(msg);
                break;
            case SFTP_RENAME:
                handle_rename(msg);
                break;
            case SFTP_REMOVE:
                handle_remove(msg);
                break;
            case SFTP_SETSTAT:
            case SFTP_FSETSTAT:
                handle_setstat(msg, type == SFTP_FSETSTAT);
                break;
            case SFTP_READLINK:
                handle_readlink(msg);
                break;
            case SFTP_SYMLINK:
                handle_symlink(msg);
                break;
            default:
                cout << "Unknown message: " << static_cast<int>(type) << "\n";
                sftp_reply_status(msg, SSH_FX_OP_UNSUPPORTED, "Unsupported message");
            }
        }
    }

private:
    using SftpHandleUPtr = std::unique_ptr<ssh_string_struct, void (*)(ssh_string)>;

    sftp_attributes_struct attr_from(const QFileInfo& file_info)
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
        attr.flags = SSH_FILEXFER_ATTR_SIZE | SSH_FILEXFER_ATTR_UIDGID | SSH_FILEXFER_ATTR_PERMISSIONS |
                     SSH_FILEXFER_ATTR_ACMODTIME;

        if (file_info.isSymLink())
        {
            attr.permissions = attr.permissions | SSH_S_IFLNK | 0777;
        }
        else if (file_info.isDir())
        {
            attr.permissions = attr.permissions | SSH_S_IFDIR;
        }
        else if (file_info.isFile())
        {
            attr.permissions = attr.permissions | SSH_S_IFREG;
        }

        return attr;
    }

    void handle_close(sftp_client_message msg)
    {
        const auto id = sftp_handle(msg->sftp, msg->handle);

        auto erased = open_file_handles.erase(id);
        erased += open_dir_handles.erase(id);

        if (erased == 0)
        {
            sftp_reply_status(msg, SSH_FX_BAD_MESSAGE, "close: invalid handle");
            return;
        }

        sftp_handle_remove(sftp_server, id);
        sftp_reply_status(msg, SSH_FX_OK, nullptr);
    }

    void handle_fstat(sftp_client_message msg)
    {
        auto file = handle_from(msg, open_file_handles);
        if (file == nullptr)
        {
            sftp_reply_status(msg, SSH_FX_BAD_MESSAGE, "fstat: invalid handle");
            return;
        }

        QFileInfo file_info(*file);

        if (file_info.isSymLink())
            file_info = QFileInfo(file_info.symLinkTarget());

        auto attr = attr_from(file_info);

        sftp_reply_attr(msg, &attr);
    }

    void handle_mkdir(sftp_client_message msg)
    {
        const auto filename = sftp_client_message_get_filename(msg);
        QDir dir(filename);

        if (!dir.mkdir(filename))
        {
            reply_status(msg);
            return;
        }

        if (!QFile::setPermissions(filename, to_qt_permissions(msg->attr->permissions)))
        {
            reply_status(msg);
            return;
        }

        QFileInfo current_dir(filename);
        QFileInfo parent_dir(current_dir.path());
        auto ret = chown(filename, parent_dir.ownerId(), parent_dir.groupId());
        if (ret < 0)
        {
            cout << fmt::format("failed to chown '{}' to owner:{} and group:{}\n", filename, parent_dir.ownerId(),
                                parent_dir.groupId());
        }
        sftp_reply_status(msg, SSH_FX_OK, nullptr);
    }

    void handle_rmdir(sftp_client_message msg)
    {
        const auto filename = sftp_client_message_get_filename(msg);
        QDir dir(filename);

        if (!dir.rmdir(filename))
        {
            reply_status(msg);
            return;
        }

        sftp_reply_status(msg, SSH_FX_OK, nullptr);
    }

    void handle_open(sftp_client_message msg)
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
                cout << "Adding sshfs O_APPEND workaround.\n";
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
        {
            reply_status(msg);
            return;
        }

        if (!exists)
        {
            if (!file->setPermissions(to_qt_permissions(msg->attr->permissions)))
            {
                reply_status(msg);
                return;
            }

            QFileInfo current_file(filename);
            QFileInfo current_dir(current_file.path());
            auto ret = chown(filename, current_dir.ownerId(), current_dir.groupId());
            if (ret < 0)
            {
                fmt::format("failed to chown '{}' to owner:{} and group:{}\n", filename, current_dir.ownerId(),
                            current_dir.groupId());
            }
        }

        SftpHandleUPtr sftp_handle{sftp_handle_alloc(sftp_server, file.get()), ssh_string_free};
        open_file_handles.emplace(file.get(), std::move(file));

        sftp_reply_handle(msg, sftp_handle.get());
    }

    void handle_opendir(sftp_client_message msg)
    {
        QDir dir(sftp_client_message_get_filename(msg));

        if (!dir.exists())
        {
            reply_status(msg, ENOENT);
            return;
        }

        if (!dir.isReadable())
        {
            reply_status(msg, EACCES);
            return;
        }

        auto entry_list =
            std::make_unique<QFileInfoList>(dir.entryInfoList(QDir::AllEntries | QDir::System | QDir::Hidden));

        SftpHandleUPtr sftp_handle{sftp_handle_alloc(sftp_server, entry_list.get()), ssh_string_free};
        open_dir_handles.emplace(entry_list.get(), std::move(entry_list));

        sftp_reply_handle(msg, sftp_handle.get());
    }

    void handle_read(sftp_client_message msg)
    {
        auto file = handle_from(msg, open_file_handles);
        if (file == nullptr)
        {
            sftp_reply_status(msg, SSH_FX_BAD_MESSAGE, "read: invalid handle");
            return;
        }

        const auto max_packet_size = 65536u;
        const auto len = std::min(msg->len, max_packet_size);

        std::vector<char> data;
        data.reserve(len);

        file->seek(msg->offset);
        auto r = file->read(data.data(), len);

        if (r < 0)
        {
            sftp_reply_status(msg, SSH_FX_FAILURE, file->errorString().toStdString().c_str());
        }
        else if (r == 0)
        {
            sftp_reply_status(msg, SSH_FX_EOF, "End of file");
            return;
        }
        else
        {
            sftp_reply_data(msg, data.data(), r);
        }
    }

    void handle_readdir(sftp_client_message msg)
    {
        auto dir_entries = handle_from(msg, open_dir_handles);
        if (dir_entries == nullptr)
        {
            sftp_reply_status(msg, SSH_FX_BAD_MESSAGE, "readdir: invalid handle");
            return;
        }

        if (dir_entries->isEmpty())
        {
            sftp_reply_status(msg, SSH_FX_EOF, nullptr);
            return;
        }

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

        sftp_reply_names(msg);
    }

    void handle_readlink(sftp_client_message msg)
    {
        auto link = QFile::symLinkTarget(sftp_client_message_get_filename(msg));

        if (link.isEmpty())
        {
            sftp_reply_status(msg, SSH_FX_NO_SUCH_FILE, "invalid link");
            return;
        }

        sftp_attributes_struct attr{};

        sftp_reply_names_add(msg, link.toStdString().c_str(), link.toStdString().c_str(), &attr);

        sftp_reply_names(msg);
    }

    void handle_realpath(sftp_client_message msg)
    {
        auto realpath = QFileInfo(sftp_client_message_get_filename(msg)).absoluteFilePath();

        sftp_reply_name(msg, realpath.toStdString().c_str(), nullptr);
    }

    void handle_remove(sftp_client_message msg)
    {
        if (!QFile::remove(sftp_client_message_get_filename(msg)))
        {
            reply_status(msg);
            return;
        }

        sftp_reply_status(msg, SSH_FX_OK, nullptr);
    }

    void handle_rename(sftp_client_message msg)
    {
        const auto target = sftp_client_message_get_data(msg);
        const auto source = sftp_client_message_get_filename(msg);
        if (QFile::exists(target))
        {
            if (!QFile::remove(target))
            {
                reply_status(msg);
                return;
            }
        }

        if (!QFile::rename(source, target))
        {
            reply_status(msg);
            return;
        }

        sftp_reply_status(msg, SSH_FX_OK, nullptr);
    }

    void handle_setstat(sftp_client_message msg, bool get_handle)
    {
        QString filename;

        if (get_handle)
        {
            auto file = handle_from(msg, open_file_handles);
            filename = file->fileName();
        }
        else
        {
            filename = sftp_client_message_get_filename(msg);
        }

        if (msg->attr->flags & SSH_FILEXFER_ATTR_SIZE)
        {
            if (!QFile::resize(filename, msg->attr->size))
            {
                reply_status(msg);
                return;
            }
        }

        if (msg->attr->flags & SSH_FILEXFER_ATTR_PERMISSIONS)
        {
            if (!QFile::setPermissions(filename, to_qt_permissions(msg->attr->permissions)))
            {
                reply_status(msg);
                return;
            }
        }

        if (msg->attr->flags & SSH_FILEXFER_ATTR_ACMODTIME)
        {
            struct utimbuf timebuf;

            timebuf.actime = msg->attr->atime;
            timebuf.modtime = msg->attr->mtime;

            if (utime(filename.toStdString().c_str(), &timebuf) < 0)
            {
                reply_status(msg);
                return;
            }
        }

        if (msg->attr->flags & SSH_FILEXFER_ATTR_UIDGID)
        {
            if (chown(filename.toStdString().c_str(), msg->attr->uid, msg->attr->gid) < 0)
            {
                reply_status(msg);
                return;
            }
        }

        sftp_reply_status(msg, SSH_FX_OK, nullptr);
    }

    void handle_stat(sftp_client_message msg, const bool follow)
    {
        QFileInfo file_info(sftp_client_message_get_filename(msg));

        if (!file_info.exists())
        {
            reply_status(msg, ENOENT);
            return;
        }

        if (follow && file_info.isSymLink())
            file_info = QFileInfo(file_info.symLinkTarget());

        auto attr = attr_from(file_info);

        sftp_reply_attr(msg, &attr);
    }

    void handle_symlink(sftp_client_message msg)
    {
        const auto old_name = sftp_client_message_get_filename(msg);
        const auto new_name = sftp_client_message_get_data(msg);
        if (!QFile::link(old_name, new_name))
        {
            reply_status(msg);
            return;
        }

        sftp_reply_status(msg, SSH_FX_OK, nullptr);
    }

    void handle_write(sftp_client_message msg)
    {
        auto file = handle_from(msg, open_file_handles);
        if (file == nullptr)
        {
            sftp_reply_status(msg, SSH_FX_BAD_MESSAGE, "write: invalid handle");
            return;
        }

        auto len = ssh_string_len(msg->data);
        auto data_ptr = ssh_string_get_char(msg->data);
        file->seek(msg->offset);

        do
        {
            auto r = file->write(data_ptr, len);
            if (r < 0)
            {
                reply_status(msg);
                return;
            }

            file->flush();

            data_ptr += r;
            len -= r;
        } while (len > 0);

        sftp_reply_status(msg, SSH_FX_OK, "");
    }

    std::unordered_map<void*, std::unique_ptr<QFileInfoList>> open_dir_handles;
    std::unordered_map<void*, std::unique_ptr<QFile>> open_file_handles;
    const sftp_session sftp_server;
    const std::unordered_map<int, int> gid_map;
    const std::unordered_map<int, int> uid_map;
    const int default_uid;
    const int default_gid;
    std::ostream& cout;
};
} // namespace anonymous

mp::SshfsMount::SshfsMount(std::function<SSHSession()> make_session, const std::string& source,
                           const std::string& target, const std::unordered_map<int, int>& gid_map,
                           const std::unordered_map<int, int>& uid_map, std::ostream& cout)
    : ssh_session{make_session()},
      sshfs_process{create_sshfs_process(ssh_session, source, target)},
      gid_map{gid_map},
      uid_map{uid_map},
      cout{cout}
{
}

mp::SshfsMount::~SshfsMount()
{
    stop();
}

void mp::SshfsMount::run()
{
    auto uid = std::stoi(run_cmd(ssh_session, "id -u"));
    auto gid = std::stoi(run_cmd(ssh_session, "id -g"));

    auto t = std::thread([this, uid, gid]() mutable {
        std::unique_ptr<ssh_session_struct, decltype(ssh_free)*> session_sftp{ssh_new(), ssh_free};

        std::unique_ptr<sftp_session_struct, decltype(sftp_free)*> sftp_server_session{
            sftp_server_new(session_sftp.get(), sshfs_process.release_channel()), sftp_free};

        SSH::throw_on_error(sftp_server_init, sftp_server_session);

        SftpServer sftp_server(sftp_server_session.get(), gid_map, uid_map, uid, gid, cout);
        sftp_server.process_sftp();

        emit finished();
    });

    mount_thread = std::move(t);
}

void mp::SshfsMount::stop()
{
    ssh_session.force_shutdown();
    if (mount_thread.joinable())
        mount_thread.join();
}
