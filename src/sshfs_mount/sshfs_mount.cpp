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

#include <multipass/ssh/throw_on_error.h>
#include <multipass/sshfs_mount/sshfs_mount.h>
#include <multipass/utils.h>

#include <libssh/sftp.h>

#include <utime.h>

#include <QDateTime>
#include <QDir>
#include <QFile>

namespace mp = multipass;

namespace
{
struct SftpHandleInfo
{
    SftpHandleInfo(int type, QFileInfoList entry_list) : type{type}, entry_list{entry_list}
    {
    }

    SftpHandleInfo(int type, std::unique_ptr<QFile> file) : type{type}, file{std::move(file)}
    {
    }

    int type;
    QFileInfoList entry_list;
    std::unique_ptr<QFile> file;
};

using SftpHandleMap = std::unordered_map<SftpHandleInfo*, std::unique_ptr<SftpHandleInfo>>;
using SftpHandleUPtr = std::unique_ptr<ssh_string_struct, void (*)(ssh_string)>;

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
        return sftp_reply_status(msg, SSH_FX_FAILURE, NULL);
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

auto handle_from_msg(sftp_client_message& msg)
{
    return reinterpret_cast<SftpHandleInfo*>(sftp_handle(msg->sftp, msg->handle));
}

auto sshfs_cmd_from(const QString& source, const QString& target)
{
    return QString("sudo sshfs -o slave -o nonempty -o transform_symlinks -o allow_other -o reconnect :%1 \"%2\"")
        .arg(format_path(source))
        .arg(target);
}

auto get_vm_user_and_group_names(mp::SSHSession* session)
{
    std::pair<std::string, std::string> vm_user_group;

    QString cmd = "id -nu";
    auto ssh_process = session->exec({cmd.toStdString()}, mp::utils::QuoteType::no_quotes);
    vm_user_group.first = ssh_process.get_output_streams()[0];

    cmd = "id -ng";
    ssh_process = session->exec({cmd.toStdString()}, mp::utils::QuoteType::no_quotes);
    vm_user_group.second = ssh_process.get_output_streams()[0];

    return vm_user_group;
}

auto create_sshfs_process(mp::SSHSession* session, const QString& target, const QString& sshfs_cmd)
{
    QString cmd(QString("sudo mkdir -p \"%1\"").arg(target));
    auto ssh_process = session->exec({cmd.toStdString()}, mp::utils::QuoteType::no_quotes);
    if (ssh_process.exit_code() != 0)
    {
        throw std::runtime_error(ssh_process.get_output_streams()[1]);
    }

    auto vm_user_group_names = get_vm_user_and_group_names(session);
    cmd = QString("sudo chown %1:%2 \"%3\"")
              .arg(QString::fromStdString(vm_user_group_names.first).simplified())
              .arg(QString::fromStdString(vm_user_group_names.second).simplified())
              .arg(target);
    ssh_process = session->exec({cmd.toStdString()}, mp::utils::QuoteType::no_quotes);
    if (ssh_process.exit_code() != 0)
    {
        throw std::runtime_error(ssh_process.get_output_streams()[1]);
    }

    return session->exec({sshfs_cmd.toStdString()}, mp::utils::QuoteType::no_quotes);
}

auto get_vm_user_pair(mp::SSHSession* session)
{
    std::pair<int, int> vm_user_pair;

    QString cmd = "id -u";
    auto ssh_process = session->exec({cmd.toStdString()}, mp::utils::QuoteType::no_quotes);
    vm_user_pair.first = std::stoi(ssh_process.get_output_streams()[0]);

    cmd = "id -g";
    ssh_process = session->exec({cmd.toStdString()}, mp::utils::QuoteType::no_quotes);
    vm_user_pair.second = std::stoi(ssh_process.get_output_streams()[0]);

    return vm_user_pair;
}

auto sshfs_pid_from(mp::SSHSession* session, const QString& source, const QString& target)
{
    using namespace std::literals::chrono_literals;

    // Make sure sshfs actually runs
    std::this_thread::sleep_for(250ms);
    QString pgrep_cmd(QString("pgrep -fx \"sshfs.*%1.*%2\"").arg(source).arg(target));
    auto ssh_process = session->exec({pgrep_cmd.toStdString()}, mp::utils::QuoteType::no_quotes);

    return QString::fromStdString(ssh_process.get_output_streams()[0]);
}

void stop_sshfs_process(mp::SSHSession* session, const QString& sshfs_pid)
{
    QString kill_cmd(QString("sudo kill %1").arg(sshfs_pid));
    session->exec({kill_cmd.toStdString()}, mp::utils::QuoteType::no_quotes);
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
            MsgUPtr msg{sftp_get_client_message(sftp_server), sftp_client_message_free};

            if (!msg)
            {
                break;
            }

            switch (msg->type)
            {
            case SFTP_REALPATH:
                handle_realpath(msg.get());
                break;
            case SFTP_OPENDIR:
                handle_opendir(msg.get());
                break;
            case SFTP_MKDIR:
                handle_mkdir(msg.get());
                break;
            case SFTP_RMDIR:
                handle_rmdir(msg.get());
                break;
            case SFTP_LSTAT:
            case SFTP_STAT:
                handle_stat(msg.get(), msg->type == SFTP_STAT);
                break;
            case SFTP_FSTAT:
                handle_fstat(msg.get());
                break;
            case SFTP_READDIR:
                handle_readdir(msg.get());
                break;
            case SFTP_CLOSE:
                handle_close(msg.get());
                break;
            case SFTP_OPEN:
                handle_open(msg.get());
                break;
            case SFTP_READ:
                handle_read(msg.get());
                break;
            case SFTP_WRITE:
                handle_write(msg.get());
                break;
            case SFTP_RENAME:
                handle_rename(msg.get());
                break;
            case SFTP_REMOVE:
                handle_remove(msg.get());
                break;
            case SFTP_SETSTAT:
            case SFTP_FSETSTAT:
                handle_setstat(msg.get(), msg->type == SFTP_FSETSTAT);
                break;
            case SFTP_READLINK:
                handle_readlink(msg.get());
                break;
            case SFTP_SYMLINK:
                handle_symlink(msg.get());
                break;
            default:
                cout << "Unknown message: " << (int)msg->type << "\n";
                sftp_reply_status(msg.get(), SSH_FX_OP_UNSUPPORTED, "Unsupported message");
            }
        }
    }

private:
    sftp_attributes_struct attr_from(const QFileInfo& file_info)
    {
        sftp_attributes_struct attr{};

        attr.size = file_info.size();

        auto map = uid_map.find(file_info.ownerId());
        if (map != uid_map.end())
        {
            attr.uid = map->second;
        }
        else
        {
            attr.uid = file_info.ownerId();
        }

        auto gmap = gid_map.find(file_info.groupId());
        if (gmap != gid_map.end())
        {
            attr.gid = gmap->second;
        }
        else
        {
            attr.gid = file_info.groupId();
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
        auto handle = handle_from_msg(msg);

        if (!handle)
        {
            sftp_reply_status(msg, SSH_FX_BAD_MESSAGE, "close: invalid handle");
            return;
        }

        sftp_handle_remove(msg->sftp, handle);

        if (handle->type == SSH_FILEXFER_TYPE_REGULAR)
        {
            handle->file->close();
        }

        handle_map.erase(handle);
        sftp_reply_status(msg, SSH_FX_OK, NULL);
    }

    void handle_fstat(sftp_client_message msg)
    {
        auto handle = handle_from_msg(msg);

        if (!handle)
        {
            sftp_reply_status(msg, SSH_FX_BAD_MESSAGE, "fstat: invalid handle");
            return;
        }

        QFileInfo file_info(*handle->file);
        auto attr = attr_from(file_info);

        sftp_reply_attr(msg, &attr);
    }

    void handle_mkdir(sftp_client_message msg)
    {
        QDir dir(sanitize_path_name(QString(msg->filename)));

        if (!dir.mkdir(msg->filename))
        {
            reply_status(msg);
            return;
        }

        if (!QFile::setPermissions(msg->filename, convert_permissions(msg->attr->permissions)))
        {
            reply_status(msg);
            return;
        }

        QFileInfo current_dir(sanitize_path_name(QString(msg->filename)));
        QFileInfo parent_dir(current_dir.path());
        auto ret = chown(msg->filename, parent_dir.ownerId(), parent_dir.groupId());
        if (ret < 0)
        {
            cout << "failed to chown " << msg->filename;
            cout << " to owner: " << parent_dir.ownerId();
            cout << " and group: " << parent_dir.groupId();
            cout << "\n";
        }
        sftp_reply_status(msg, SSH_FX_OK, NULL);
    }

    void handle_rmdir(sftp_client_message msg)
    {
        QDir dir(sanitize_path_name(QString(msg->filename)));

        if (!dir.rmdir(sanitize_path_name(QString(msg->filename))))
        {
            reply_status(msg);
            return;
        }

        sftp_reply_status(msg, SSH_FX_OK, NULL);
    }

    void handle_open(sftp_client_message msg)
    {
        QIODevice::OpenMode mode = 0;

        if (msg->flags & SSH_FXF_READ)
            mode |= QIODevice::ReadOnly;

        if (msg->flags & SSH_FXF_WRITE)
        {
            mode |= QIODevice::WriteOnly;

            // This is needed to workaround an issue where sshfs does not pass through
            // O_APPEND.  This is fixed in sshfs v. 3.2.
            // Note: This goes against the default behavior of open().
            if (msg->flags == SSH_FXF_WRITE)
            {
                mode |= QIODevice::Append;
                cout << "Adding sshfs O_APPEND workaround." << std::endl;
            }
        }

        if (msg->flags & SSH_FXF_APPEND)
            mode |= QIODevice::Append;

        if (msg->flags & SSH_FXF_TRUNC)
            mode |= QIODevice::Truncate;

        auto file = std::make_unique<QFile>(sanitize_path_name(QString(msg->filename)));

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

            QFileInfo current_file(sanitize_path_name(QString(msg->filename)));
            QFileInfo current_dir(current_file.path());
            auto ret = chown(msg->filename, current_dir.ownerId(), current_dir.groupId());
            if (ret < 0)
            {
                cout << "failed to chown " << msg->filename;
                cout << " to owner: " << current_dir.ownerId();
                cout << " and group: " << current_dir.groupId();
                cout << "\n";
            }
        }

        auto hdl = std::make_unique<SftpHandleInfo>(SSH_FILEXFER_TYPE_REGULAR, std::move(file));

        SftpHandleUPtr handle{sftp_handle_alloc(msg->sftp, hdl.get()), ssh_string_free};
        sftp_reply_handle(msg, handle.get());

        handle_map[hdl.get()] = std::move(hdl);
    }

    void handle_opendir(sftp_client_message msg)
    {
        QDir dir(sanitize_path_name(QString(msg->filename)));

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

        auto entry_list = dir.entryInfoList(QDir::AllEntries | QDir::System | QDir::Hidden);

        auto hdl = std::make_unique<SftpHandleInfo>(SSH_FILEXFER_TYPE_DIRECTORY, entry_list);

        SftpHandleUPtr handle{sftp_handle_alloc(msg->sftp, hdl.get()), ssh_string_free};
        sftp_reply_handle(msg, handle.get());

        handle_map[hdl.get()] = std::move(hdl);
    }

    void handle_read(sftp_client_message msg)
    {
        auto handle = handle_from_msg(msg);

        if (!handle || handle->type != SSH_FILEXFER_TYPE_REGULAR)
        {
            sftp_reply_status(msg, SSH_FX_BAD_MESSAGE, "read: invalid handle");
            return;
        }

        auto len = msg->len;
        if (len > (2 << 15))
        {
            /* 32000 */
            len = 2 << 15;
        }

        QByteArray data(len, '\0');

        handle->file->seek(msg->offset);
        auto r = handle->file->read(data.data(), len);

        if (r <= 0 && (len > 0))
        {
            sftp_reply_status(msg, SSH_FX_EOF, "End of file");
            return;
        }

        sftp_reply_data(msg, data.constData(), r);
    }

    void handle_readdir(sftp_client_message msg)
    {
        auto handle = handle_from_msg(msg);

        if (!handle || handle->type != SSH_FILEXFER_TYPE_DIRECTORY)
        {
            sftp_reply_status(msg, SSH_FX_BAD_MESSAGE, "readdir: invalid handle");
            return;
        }

        auto count{0};

        // Send at most 50 directory entries to avoid ssh packet size overflow
        while (!handle->entry_list.isEmpty() && count < 50)
        {
            auto entry = handle->entry_list.takeFirst();
            auto attr = attr_from(entry);
            auto longname = longname_from(entry);
            sftp_reply_names_add(msg, entry.fileName().toStdString().c_str(), longname.toStdString().c_str(), &attr);

            ++count;
        }

        if (count > 0)
        {
            sftp_reply_names(msg);
        }
        else
        {
            sftp_reply_status(msg, SSH_FX_EOF, NULL);
        }
    }

    void handle_readlink(sftp_client_message msg)
    {
        auto link = QFile::symLinkTarget(sanitize_path_name(QString(msg->filename)));

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
        auto realpath = QFileInfo(sanitize_path_name(QString(msg->filename))).absoluteFilePath();

        sftp_reply_name(msg, realpath.toStdString().c_str(), NULL);
    }

    void handle_remove(sftp_client_message msg)
    {
        if (!QFile::remove(sanitize_path_name(QString(msg->filename))))
        {
            reply_status(msg);
            return;
        }

        sftp_reply_status(msg, SSH_FX_OK, NULL);
    }

    void handle_rename(sftp_client_message msg)
    {
        if (QFile::exists(ssh_string_get_char(msg->data)))
        {
            if (!QFile::remove(ssh_string_get_char(msg->data)))
            {
                reply_status(msg);
                return;
            }
        }

        if (!QFile::rename(sanitize_path_name(QString(msg->filename)), ssh_string_get_char(msg->data)))
        {
            reply_status(msg);
            return;
        }

        sftp_reply_status(msg, SSH_FX_OK, NULL);
    }

    void handle_setstat(sftp_client_message msg, bool get_handle)
    {
        QString filename;

        if (get_handle)
        {
            auto handle = handle_from_msg(msg);
            filename = handle->file->fileName();
        }
        else
        {
            filename = sanitize_path_name(QString(msg->filename));
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

        sftp_reply_status(msg, SSH_FX_OK, NULL);
    }

    void handle_stat(sftp_client_message msg, const bool follow)
    {
        QFileInfo file_info(sanitize_path_name(QString(msg->filename)));

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
        if (!QFile::link(sanitize_path_name(QString(msg->filename)), ssh_string_get_char(msg->data)))
        {
            reply_status(msg);
            return;
        }

        sftp_reply_status(msg, SSH_FX_OK, NULL);
    }

    void handle_write(sftp_client_message msg)
    {
        auto handle = handle_from_msg(msg);

        if (!handle || handle->type != SSH_FILEXFER_TYPE_REGULAR)
        {
            sftp_reply_status(msg, SSH_FX_BAD_MESSAGE, "write: invalid handle");
            return;
        }

        auto len = ssh_string_len(msg->data);
        auto data_ptr = ssh_string_get_char(msg->data);
        handle->file->seek(msg->offset);

        do
        {
            auto r = handle->file->write(data_ptr, len);

            if (r < 0)
            {
                reply_status(msg);
                return;
            }

            data_ptr += r;
            len -= r;
        } while (len > 0);

        sftp_reply_status(msg, SSH_FX_OK, "");
    }

    SftpHandleMap handle_map;
    const sftp_session sftp_server;
    const std::unordered_map<int, int> gid_map;
    const std::unordered_map<int, int> uid_map;
    const std::pair<int, int> vm_user_pair;
    std::ostream& cout;
};
} // namespace anonymous

mp::SshfsMount::SshfsMount(std::function<std::unique_ptr<SSHSession>()> session_factory, const QString& source,
                           const QString& target, const std::unordered_map<int, int>& gid_map,
                           const std::unordered_map<int, int>& uid_map, std::ostream& cout)
    : session_factory{session_factory},
      ssh_session{this->session_factory()},
      source{source},
      target{target},
      sshfs_cmd{sshfs_cmd_from(source, target)},
      sshfs_process{create_sshfs_process(this->ssh_session.get(), target, sshfs_cmd)},
      sshfs_pid{sshfs_pid_from(this->ssh_session.get(), source, target)},
      gid_map{gid_map},
      uid_map{uid_map},
      cout{cout}
{
    if (sshfs_pid.isEmpty())
    {
        if (sshfs_process.exit_code() == 127)
            throw sshfs_process.exit_code();
        else
            throw std::runtime_error(sshfs_process.get_output_streams()[1]);
    }
}

mp::SshfsMount::~SshfsMount()
{
    stop();
}

void mp::SshfsMount::run()
{
    auto vm_user_pair = get_vm_user_pair(ssh_session.get());

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
        auto session = session_factory();

        stop_sshfs_process(session.get(), sshfs_pid);
    }
    catch (const std::exception& e)
    {
        cout << e.what() << std::endl;
    }

    if (mount_thread.joinable())
    {
        mount_thread.join();
    }
}
