/*
 * Copyright (C) 2022 Canonical, Ltd.
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

#ifndef MULTIPASS_MOUNT_HANDLER_H
#define MULTIPASS_MOUNT_HANDLER_H

#include <multipass/id_mappings.h>
#include <multipass/ssh/ssh_key_provider.h>

#include <string>

namespace multipass
{
class VirtualMachine;

class MountHandler
{
public:
    virtual ~MountHandler() = default;

    virtual void start_mount(VirtualMachine* vm, const std::string& source_path, const std::string& target_path,
                             const id_mappings& gid_mappings, const id_mappings& uid_mappings) = 0;
    virtual bool stop_mount(const std::string& instance, const std::string& path) = 0;
    virtual void stop_all_mounts_for_instance(const std::string& instance) = 0;
    virtual bool has_instance_already_mounted(const std::string& instance, const std::string& path) const = 0;

protected:
    MountHandler(const SSHKeyProvider& ssh_key_provider) : key(ssh_key_provider.private_key_as_base64()){};
    const std::string key;
};

} // namespace multipass
#endif // MULTIPASS_MOUNT_HANDLER_H
