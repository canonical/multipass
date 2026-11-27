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

#pragma once

#include <multipass/ssh/plain_ssh_session.h>
#include <multipass/ssh/sftp_client.h>

#include <filesystem>
#include <functional>
#include <iostream>

#include <QFlags>

struct sftp_session_struct;

namespace multipass
{
namespace fs = std::filesystem;

using SFTPSessionUPtr = std::unique_ptr<sftp_session_struct, std::function<void(sftp_session)>>;

class PlainSFTPClient : public SFTPClient, public PrivatePassProvider<PlainSFTPClient>
{
public:
    PlainSFTPClient(PlainSSHSession&& ssh_session_obj);
    ~PlainSFTPClient() override = default;

    bool is_remote_dir(const fs::path& path) override;
    bool push(const fs::path& source_path,
              const fs::path& target_path,
              SFTPClient::Flags flags) override;
    bool pull(const fs::path& source_path,
              const fs::path& target_path,
              SFTPClient::Flags flags) override;
    void from_cin(std::istream& cin, const fs::path& target_path, bool make_parent) override;
    void to_cout(const fs::path& source_path, std::ostream& cout) override;

private:
    void push_file(const fs::path& source_path, const fs::path& target_path);
    void pull_file(const fs::path& source_path, const fs::path& target_path);
    bool push_dir(const fs::path& source_path, const fs::path& target_path);
    bool pull_dir(const fs::path& source_path, const fs::path& target_path);
    void do_push_file(std::istream& source, const fs::path& target_path);
    void do_pull_file(const fs::path& source_path, std::ostream& target);

    PlainSSHSession plain_ssh_session;
    SFTPSessionUPtr sftp;
};
} // namespace multipass
