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

#ifndef MULTIPASS_SFTP_SERVER_H
#define MULTIPASS_SFTP_SERVER_H

#include <multipass/file_ops.h>
#include <multipass/id_mappings.h>
#include <multipass/recursive_dir_iterator.h>
#include <multipass/ssh/ssh_session.h>

#include <libssh/sftp.h>

#include <memory>
#include <unordered_map>

#include <QFile>
#include <QFileInfo>

namespace multipass
{
class SSHSession;
class SSHProcess;

class SftpServer
{
public:
    SftpServer(SSHSession&& ssh_session, const std::string& source, const std::string& target,
               const id_mappings& gid_mappings, const id_mappings& uid_mappings, int default_uid, int default_gid,
               const std::string& sshfs_exec_line);
    SftpServer(SftpServer&& other);
    ~SftpServer();

    void run();
    void stop();

    using SSHSessionUptr = std::unique_ptr<ssh_session_struct, decltype(ssh_free)*>;
    using SftpSessionUptr = std::unique_ptr<sftp_session_struct, decltype(sftp_free)*>;
    using SSHFSProcUptr = std::unique_ptr<SSHProcess>;

private:
    void process_message(sftp_client_message msg);
    sftp_attributes_struct attr_from(const QFileInfo& file_info);
    int mapped_uid_for(const int uid);
    int mapped_gid_for(const int gid);
    int reverse_uid_for(const int uid, const int default_id);
    int reverse_gid_for(const int gid, const int default_id);
    bool has_uid_mapping_for(const int uid);
    bool has_gid_mapping_for(const int gid);
    bool has_reverse_uid_mapping_for(const int uid);
    bool has_reverse_gid_mapping_for(const int gid);
    bool has_id_mappings_for(const QFileInfo& file_info);

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

    template <typename T>
    T* get_handle(sftp_client_message msg);

    SSHSession ssh_session;
    SSHFSProcUptr sshfs_process;
    SftpSessionUptr sftp_server_session;
    const std::string source_path;
    const std::string target_path;
    std::unordered_map<void*, std::unique_ptr<NamedFd>> open_file_handles;
    std::unordered_map<void*, std::unique_ptr<DirIterator>> open_dir_handles;
    const id_mappings gid_mappings;
    const id_mappings uid_mappings;
    const int default_uid;
    const int default_gid;
    const std::string sshfs_exec_line;
    bool stop_invoked{false};
};
} // namespace multipass
#endif // MULTIPASS_SFTP_SERVER_H
