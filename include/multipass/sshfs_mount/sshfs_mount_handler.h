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

#ifndef MULTIPASS_SSHFS_MOUNT_HANDLER_H
#define MULTIPASS_SSHFS_MOUNT_HANDLER_H

#include <multipass/mount_handler.h>
#include <multipass/process/process.h>
#include <multipass/sshfs_server_config.h>

namespace multipass
{
struct SSHFSMountHandler : public MountHandler
{
    SSHFSMountHandler(VirtualMachine* vm, const SSHKeyProvider* ssh_key_provider, std::string target,
                      const VMMount& mount);
    ~SSHFSMountHandler() override;

    void start(ServerVariant server, std::chrono::milliseconds timeout = std::chrono::minutes(5)) override;
    void stop() override;

private:
    SSHFSServerConfig config{};
    std::unique_ptr<Process> process;
};
} // namespace multipass
#endif // MULTIPASS_SSHFS_MOUNT_HANDLER_H
