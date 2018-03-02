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

#ifndef MULTIPASS_SFTP_SERVER_H
#define MULTIPASS_SFTP_SERVER_H

#include <multipass/ssh/ssh_session.h>

#include <libssh/sftp.h>

#include <memory>
#include <unordered_map>

#include <QFile>
#include <QFileInfoList>

namespace multipass
{
class SSHSession;
class SftpServer
{
public:
    SftpServer(SSHSession&& ssh_session, SSHProcess&& sshfs_proc, const std::string& source,
               const std::unordered_map<int, int>& gid_map, const std::unordered_map<int, int>& uid_map,
               int default_uid, int default_gid, std::ostream& cout);
    SftpServer(SftpServer&& other);

    void run();
    void stop();

    using SSHSessionUptr = std::unique_ptr<ssh_session_struct, decltype(ssh_free)*>;
    using SftpSessionUptr = std::unique_ptr<sftp_session_struct, decltype(sftp_free)*>;

private:
    void process_message(sftp_client_message msg);
    sftp_attributes_struct attr_from(const QFileInfo& file_info);

    int handle_close(sftp_client_message msg);
    int handle_fstat(sftp_client_message msg);
    int handle_mkdir(sftp_client_message msg);
    int handle_rmdir(sftp_client_message msg);
    int handle_open(sftp_client_message msg);
    int handle_opendir(sftp_client_message msg);
    int handle_read(sftp_client_message msg);
    int handle_readdir(sftp_client_message msg);
    int handle_readlink(sftp_client_message msg);
    int handle_realpath(sftp_client_message msg);
    int handle_remove(sftp_client_message msg);
    int handle_rename(sftp_client_message msg);
    int handle_setstat(sftp_client_message msg);
    int handle_stat(sftp_client_message msg, bool follow);
    int handle_symlink(sftp_client_message msg);
    int handle_write(sftp_client_message msg);
    int handle_extended(sftp_client_message msg);

    SSHSession ssh_session;
    const SSHSessionUptr sftp_ssh_session;
    const SftpSessionUptr sftp_server_session;
    const std::string source_path;
    std::unordered_map<void*, std::unique_ptr<QFileInfoList>> open_dir_handles;
    std::unordered_map<void*, std::unique_ptr<QFile>> open_file_handles;
    const std::unordered_map<int, int> gid_map;
    const std::unordered_map<int, int> uid_map;
    const int default_uid;
    const int default_gid;
    std::ostream& cout;
};
} // namespace multipass
#endif // MULTIPASS_SFTP_SERVER_H
