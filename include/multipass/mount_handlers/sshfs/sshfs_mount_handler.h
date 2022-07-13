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

#include <multipass/mount_handlers/mount_handler.h>
#include <multipass/process/process.h>
#include <multipass/qt_delete_later_unique_ptr.h>
#include <multipass/ssh/ssh_key_provider.h>

#include <unordered_map>

namespace multipass
{
class VirtualMachine;

class SSHFSMountHandler : public QObject, public MountHandler
{
    Q_OBJECT
public:
    explicit SSHFSMountHandler(const SSHKeyProvider& ssh_key_provider);

    void start_mount(VirtualMachine* vm, ServerVariant server, const std::string& source_path,
                     const std::string& target_path, const id_mappings& gid_mappings, const id_mappings& uid_mappings,
                     const std::chrono::milliseconds& timeout = std::chrono::minutes(5)) override;

    bool stop_mount(const std::string& instance, const std::string& path) override;
    void stop_all_mounts_for_instance(const std::string& instance) override;

    bool has_instance_already_mounted(const std::string& instance, const std::string& path) const override;

private:
    std::unordered_map<std::string, std::unordered_map<std::string, qt_delete_later_unique_ptr<Process>>>
        mount_processes;
};

} // namespace multipass
#endif // MULTIPASS_SSHFS_MOUNT_HANDLER_H
