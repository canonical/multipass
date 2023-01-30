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

#ifndef MULTIPASS_SFTP_CLIENT_H
#define MULTIPASS_SFTP_CLIENT_H

#include "ssh_session.h"

#include <libssh/sftp.h>

#include <filesystem>
#include <functional>
#include <iostream>

#include <QFlags>

namespace multipass
{
namespace fs = std::filesystem;

using SSHSessionUPtr = std::unique_ptr<SSHSession>;
using SFTPSessionUPtr = std::unique_ptr<sftp_session_struct, std::function<void(sftp_session)>>;

SFTPSessionUPtr make_sftp_session(ssh_session session);

class SFTPClient
{
public:
    enum class Flag
    {
        Recursive = 1,
        MakeParent = 2,
    };
    Q_DECLARE_FLAGS(Flags, Flag)

    SFTPClient() = default;
    SFTPClient(const std::string& host, int port, const std::string& username, const std::string& priv_key_blob);
    SFTPClient(SSHSessionUPtr ssh_session);

    virtual bool is_remote_dir(const fs::path& path);
    virtual bool push(const fs::path& source_path, const fs::path& target_path, Flags flags = {});
    virtual bool pull(const fs::path& source_path, const fs::path& target_path, Flags flags = {});
    virtual void from_cin(std::istream& cin, const fs::path& target_path, bool make_parent);
    virtual void to_cout(const fs::path& source_path, std::ostream& cout);

    virtual ~SFTPClient() = default;

private:
    void push_file(const fs::path& source_path, const fs::path& target_path);
    void pull_file(const fs::path& source_path, const fs::path& target_path);
    bool push_dir(const fs::path& source_path, const fs::path& target_path);
    bool pull_dir(const fs::path& source_path, const fs::path& target_path);
    void do_push_file(std::istream& source, const fs::path& target_path);
    void do_pull_file(const fs::path& source_path, std::ostream& target);

    SSHSessionUPtr ssh_session;
    SFTPSessionUPtr sftp;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(SFTPClient::Flags)
} // namespace multipass
#endif // MULTIPASS_SFTP_CLIENT_H
