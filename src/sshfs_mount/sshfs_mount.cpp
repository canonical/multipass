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

#include <utime.h>

#include <QDateTime>
#include <QDir>
#include <QFile>

namespace mp = multipass;

namespace
{
QString sanitize_path_name(const QString& path_name)
{
    if (path_name.contains(" "))
    {
        QString sanitized_path(path_name);
        sanitized_path.remove("\"");
        return sanitized_path;
    }

    return path_name;
}

QString format_path(const QString& path)
{
    if (path.contains(" "))
    {
        return QString("\'\"%1\"\'").arg(path);
    }

    return path;
}

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

QString longname_from(const QFileInfo& file_info)
{
    QString buff;
    auto mode = file_info.permissions();

    if (file_info.isSymLink())
        buff.append("l");
    else if (file_info.isDir())
        buff.append("d");
    else
        buff.append("-");

    /* user */
    if (mode & QFileDevice::ReadUser)
        buff.append("r");
    else
        buff.append("-");

    if (mode & QFileDevice::WriteUser)
        buff.append("w");
    else
        buff.append("-");

    if (mode & QFileDevice::ExeUser)
    {
        buff.append("x");
    }
    else
    {
        buff.append("-");
    }

    /*group*/
    if (mode & QFileDevice::ReadGroup)
        buff.append("r");
    else
        buff.append("-");

    if (mode & QFileDevice::WriteGroup)
        buff.append("w");
    else
        buff.append("-");

    if (mode & QFileDevice::ExeGroup)
        buff.append("x");
    else
        buff.append("-");

    /* other */
    if (mode & QFileDevice::ReadOther)
        buff.append("r");
    else
        buff.append("-");

    if (mode & QFileDevice::WriteOther)
        buff.append("w");
    else
        buff.append("-");

    if (mode & QFileDevice::ExeOther)
        buff.append("x");
    else
        buff.append("-");

    buff.append(QString(" 1 %1 %2 %3").arg(file_info.ownerId()).arg(file_info.groupId()).arg(file_info.size()));

    buff.append(
        QString(" %1 %2").arg(file_info.lastModified().toString("MMM d hh:mm:ss yyyy")).arg(file_info.fileName()));

    return buff;
}

// Convert passed in octal to same hex digits for Qt Permissions flags
auto convert_permissions(int perms)
{
    return (QFileDevice::Permissions)(QString::number(perms & 07777, 8).toUInt(nullptr, 16));
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

auto sshfs_cmd_from(const QString& source, const QString& target)
{
    return QString("sudo sshfs -o slave -o nonempty -o transform_symlinks -o allow_other -o reconnect :%1 \"%2\"")
        .arg(format_path(source))
        .arg(target);
}

auto get_vm_user_and_group_names(mp::SSHSession& session)
{
    std::pair<std::string, std::string> vm_user_group;

    QString cmd = "id -nu";
    auto ssh_process = session.exec({cmd.toStdString()});
    vm_user_group.first = ssh_process.read_std_output();

    cmd = "id -ng";
    ssh_process = session.exec({cmd.toStdString()});
    vm_user_group.second = ssh_process.read_std_output();

    return vm_user_group;
}

auto create_sshfs_process(mp::SSHSession& session, const QString& target, const QString& sshfs_cmd)
{
    QString cmd("which sshfs");
    auto ssh_process = session.exec({cmd.toStdString()});
    if (ssh_process.exit_code() != 0)
        throw mp::SSHFSMissingError();

    cmd = QString("sudo mkdir -p \"%1\"").arg(target);
    ssh_process = session.exec({cmd.toStdString()});
    if (ssh_process.exit_code() != 0)
    {
        throw std::runtime_error(ssh_process.read_std_error());
    }

    auto vm_user_group_names = get_vm_user_and_group_names(session);
    cmd = QString("sudo chown %1:%2 \"%3\"")
              .arg(QString::fromStdString(vm_user_group_names.first).simplified())
              .arg(QString::fromStdString(vm_user_group_names.second).simplified())
              .arg(target);
    ssh_process = session.exec({cmd.toStdString()});
    if (ssh_process.exit_code() != 0)
    {
        throw std::runtime_error(ssh_process.read_std_error());
    }

    return session.exec({sshfs_cmd.toStdString()});
}

auto get_vm_user_pair(mp::SSHSession& session)
{
    std::pair<int, int> vm_user_pair;

    QString cmd = "id -u";
    auto ssh_process = session.exec({cmd.toStdString()});
    vm_user_pair.first = std::stoi(ssh_process.read_std_output());

    cmd = "id -g";
    ssh_process = session.exec({cmd.toStdString()});
    vm_user_pair.second = std::stoi(ssh_process.read_std_output());

    return vm_user_pair;
}

auto sshfs_pid_from(mp::SSHSession& session, const QString& source, const QString& target)
{
    using namespace std::literals::chrono_literals;

    // Make sure sshfs actually runs
    std::this_thread::sleep_for(250ms);
    QString pgrep_cmd(QString("pgrep -fx \"sshfs.*%1.*%2\"").arg(source).arg(target));
    auto ssh_process = session.exec({pgrep_cmd.toStdString()});

    return QString::fromStdString(ssh_process.read_std_output());
}

void stop_sshfs_process(mp::SSHSession& session, const QString& sshfs_pid)
{
    QString kill_cmd(QString("sudo kill %1").arg(sshfs_pid));
    session.exec({kill_cmd.toStdString()});
}

class SftpServer
{
public:
    SftpServer(const sftp_session& sftp_server_session, const std::unordered_map<int, int>& gid_map,
               const std::unordered_map<int, int>& uid_map, const std::pair<int, int>& vm_user_pair, std::ostream& cout)
        : sftp_server{sftp_server_session}, gid_map{gid_map}, uid_map{uid_map}, vm_user_pair{vm_user_pair}, cout{cout}
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
            attr.uid = vm_user_pair.first;
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
            attr.gid = vm_user_pair.second;
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

        attr.permissions = QString::number(file_info.permissions() & 07777, 16).toUInt(nullptr, 8);
        attr.atime = file_info.lastRead().toUTC().toMSecsSinceEpoch() / 1000;
        attr.mtime = file_info.lastModified().toUTC().toMSecsSinceEpoch() / 1000;
        attr.flags = SSH_FILEXFER_ATTR_SIZE | SSH_FILEXFER_ATTR_UIDGID | SSH_FILEXFER_ATTR_PERMISSIONS |
                     SSH_FILEXFER_ATTR_ACMODTIME;

        if (file_info.isDir())
        {
            attr.permissions = attr.permissions | SSH_S_IFDIR;
        }

        if (file_info.isFile())
        {
            attr.permissions = attr.permissions | SSH_S_IFREG;
        }

        if (file_info.isSymLink())
        {
            attr.permissions = attr.permissions | SSH_S_IFLNK;
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
        auto attr = attr_from(file_info);

        sftp_reply_attr(msg, &attr);
    }

    void handle_mkdir(sftp_client_message msg)
    {
        const auto filename = sftp_client_message_get_filename(msg);
        QDir dir(sanitize_path_name(filename));

        if (!dir.mkdir(filename))
        {
            reply_status(msg);
            return;
        }

        if (!QFile::setPermissions(filename, convert_permissions(msg->attr->permissions)))
        {
            reply_status(msg);
            return;
        }

        QFileInfo current_dir(sanitize_path_name(filename));
        QFileInfo parent_dir(current_dir.path());
        auto ret = chown(filename, parent_dir.ownerId(), parent_dir.groupId());
        if (ret < 0)
        {
            cout << "failed to chown " << filename;
            cout << " to owner: " << parent_dir.ownerId();
            cout << " and group: " << parent_dir.groupId();
            cout << "\n";
        }
        sftp_reply_status(msg, SSH_FX_OK, nullptr);
    }

    void handle_rmdir(sftp_client_message msg)
    {
        const auto filename = sftp_client_message_get_filename(msg);
        QDir dir(sanitize_path_name(filename));

        if (!dir.rmdir(sanitize_path_name(filename)))
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
        auto file = std::make_unique<QFile>(sanitize_path_name(filename));

        auto exists = file->exists();

        if (!file->open(mode))
        {
            reply_status(msg);
            return;
        }

        if (!exists)
        {
            if (!file->setPermissions(convert_permissions(msg->attr->permissions)))
            {
                reply_status(msg);
                return;
            }

            QFileInfo current_file(sanitize_path_name(filename));
            QFileInfo current_dir(current_file.path());
            auto ret = chown(filename, current_dir.ownerId(), current_dir.groupId());
            if (ret < 0)
            {
                cout << "failed to chown " << filename;
                cout << " to owner: " << current_dir.ownerId();
                cout << " and group: " << current_dir.groupId();
                cout << "\n";
            }
        }

        SftpHandleUPtr sftp_handle{sftp_handle_alloc(sftp_server, file.get()), ssh_string_free};
        open_file_handles.emplace(file.get(), std::move(file));

        sftp_reply_handle(msg, sftp_handle.get());
    }

    void handle_opendir(sftp_client_message msg)
    {
        QDir dir(sanitize_path_name(sftp_client_message_get_filename(msg)));

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
            auto longname = longname_from(entry);
            sftp_reply_names_add(msg, entry.fileName().toStdString().c_str(), longname.toStdString().c_str(), &attr);
        }

        sftp_reply_names(msg);
    }

    void handle_readlink(sftp_client_message msg)
    {
        auto link = QFile::symLinkTarget(sanitize_path_name(sftp_client_message_get_filename(msg)));

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
        auto realpath = QFileInfo(sanitize_path_name(sftp_client_message_get_filename(msg))).absoluteFilePath();

        sftp_reply_name(msg, realpath.toStdString().c_str(), nullptr);
    }

    void handle_remove(sftp_client_message msg)
    {
        if (!QFile::remove(sanitize_path_name(sftp_client_message_get_filename(msg))))
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

        if (!QFile::rename(sanitize_path_name(source), target))
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
            filename = sanitize_path_name(sftp_client_message_get_filename(msg));
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
            if (!QFile::setPermissions(filename, convert_permissions(msg->attr->permissions)))
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
        QFileInfo file_info(sanitize_path_name(sftp_client_message_get_filename(msg)));

        if (!file_info.exists())
        {
            reply_status(msg, ENOENT);
            return;
        }

        auto attr = attr_from(file_info);

        sftp_reply_attr(msg, &attr);
    }

    void handle_symlink(sftp_client_message msg)
    {
        const auto old_name = sanitize_path_name(sftp_client_message_get_filename(msg));
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
    const std::pair<int, int> vm_user_pair;
    std::ostream& cout;
};
} // namespace anonymous

mp::SshfsMount::SshfsMount(std::function<SSHSession()> session_factory, const QString& source, const QString& target,
                           const std::unordered_map<int, int>& gid_map, const std::unordered_map<int, int>& uid_map,
                           std::ostream& cout)
    : make_session{session_factory},
      ssh_session{make_session()},
      sshfs_process{create_sshfs_process(ssh_session, target, sshfs_cmd_from(source, target))},
      sshfs_pid{sshfs_pid_from(ssh_session, source, target)},
      gid_map{gid_map},
      uid_map{uid_map},
      cout{cout}
{
    if (sshfs_pid.isEmpty())
    {
        throw std::runtime_error(sshfs_process.read_std_error());
    }
}

mp::SshfsMount::~SshfsMount()
{
    stop();
}

void mp::SshfsMount::run()
{
    auto vm_user_pair = get_vm_user_pair(ssh_session);

    auto t = std::thread([this, vm_user_pair]() mutable {
        std::unique_ptr<ssh_session_struct, decltype(ssh_free)*> session_sftp{ssh_new(), ssh_free};

        std::unique_ptr<sftp_session_struct, decltype(sftp_free)*> sftp_server_session{
            sftp_server_new(session_sftp.get(), sshfs_process.release_channel()), sftp_free};

        SSH::throw_on_error(sftp_server_init, sftp_server_session);

        SftpServer sftp_server(sftp_server_session.get(), gid_map, uid_map, vm_user_pair, cout);
        sftp_server.process_sftp();

        emit finished();
    });

    mount_thread = std::move(t);
}

void mp::SshfsMount::stop()
{
    try
    {
        auto session = make_session();
        stop_sshfs_process(session, sshfs_pid);
    }
    catch (const std::exception& e)
    {
        cout << e.what() << "\n";
    }

    if (mount_thread.joinable())
    {
        mount_thread.join();
    }
}
