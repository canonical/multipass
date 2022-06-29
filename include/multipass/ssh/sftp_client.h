/*
 * Copyright (C) 2019-2022 Canonical, Ltd.
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

#ifndef MULTIPASS_SFTP_CLIENT_H
#define MULTIPASS_SFTP_CLIENT_H

#include <multipass/ssh/ssh_session.h>

#include <libssh/sftp.h>

#include <QFlags>
#include <filesystem>
#include <functional>
#include <iostream>
#include <memory>
#include <string>

namespace multipass
{
using SSHSessionUPtr = std::unique_ptr<SSHSession>;
using SFTPSessionUPtr = std::unique_ptr<sftp_session_struct, std::function<void(sftp_session)>>;

namespace fs = std::filesystem;

enum TransferFlags
{
    Recursive = 1,
};

SFTPSessionUPtr make_sftp_session(ssh_session session);
std::unique_ptr<std::ostream> open_write_file(const fs::path& path);
std::unique_ptr<std::istream> open_read_file(const fs::path& path);
std::string get_full_file_target(const fs::path& source_path, const fs::path& target_path);
std::string get_full_file_target(sftp_session sftp, const fs::path& source_path, const fs::path& target_path);
fs::path get_full_dir_target(const fs::path& source_path, const fs::path& target_path);
fs::path get_full_dir_target(sftp_session sftp, const fs::path& source_path, const fs::path& target_path);

class SFTPClient
{
public:
    SFTPClient(const std::string& host, int port, const std::string& username, const std::string& priv_key_blob);
    SFTPClient(SSHSessionUPtr ssh_session);

    bool is_dir(const fs::path& path);
    bool push(const fs::path& source_path, const fs::path& target_path, QFlags<TransferFlags> flags,
              std::ostream& err_sink);
    bool pull(const fs::path& source_path, const fs::path& target_path, QFlags<TransferFlags> flags,
              std::ostream& err_sink);
    void push_file(const fs::path& source_path, const fs::path& target_path);
    void pull_file(const fs::path& source_path, const fs::path& target_path);
    bool push_dir(const fs::path& source_path, const fs::path& target_path, std::ostream& err_sink);
    bool pull_dir(const fs::path& source_path, const fs::path& target_path, std::ostream& err_sink);
    void from_cin(std::istream& cin, const fs::path& target_path);
    void to_cout(const fs::path& source_path, std::ostream& cout);

private:
    void do_push_file(std::istream& source, const fs::path& target_path);
    void do_pull_file(const fs::path& source_path, std::ostream& target);

    SSHSessionUPtr ssh_session;
    SFTPSessionUPtr sftp;
};
} // namespace multipass
#endif // MULTIPASS_SFTP_CLIENT_H
