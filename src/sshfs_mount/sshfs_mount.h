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

#ifndef MULTIPASS_SSHFS_MOUNT
#define MULTIPASS_SSHFS_MOUNT

#include <multipass/id_mappings.h>

#include <memory>
#include <thread>
#include <unordered_map>

namespace multipass
{
class SSHSession;
class SftpServer;
class SshfsMount
{
public:
    SshfsMount(SSHSession&& session, const std::string& source, const std::string& target,
               const id_mappings& gid_mappings, const id_mappings& uid_mappings);
    SshfsMount(SshfsMount&& other);
    ~SshfsMount();

    void stop();

private:
    // sftp_server Doesn't need to be a pointer, but done for now to avoid bringing sftp.h
    // which has an error with -pedantic.
    std::unique_ptr<SftpServer> sftp_server;
    std::thread sftp_thread;
};
} // namespace multipass
#endif // MULTIPASS_SSHFS_MOUNT
