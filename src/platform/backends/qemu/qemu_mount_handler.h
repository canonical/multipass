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

#ifndef MULTIPASS_QEMU_MOUNT_HANDLER_H
#define MULTIPASS_QEMU_MOUNT_HANDLER_H

#include <multipass/mount_handler.h>

namespace multipass
{
class VirtualMachine;

class QemuMountHandler : public MountHandler
{
public:
    explicit QemuMountHandler(const SSHKeyProvider& ssh_key_provider);

    void init_mount(VirtualMachine* vm, const std::string& target_path, const VMMount& vm_mount) override;
    void start_mount(VirtualMachine* vm, ServerVariant server, const std::string& target_path,
                     const std::chrono::milliseconds& timeout = std::chrono::minutes(5)) override;
    void stop_mount(const std::string& instance, const std::string& path) override;
    void stop_all_mounts_for_instance(const std::string& instance) override;
    bool has_instance_already_mounted(const std::string& instance, const std::string& path) const override;

private:
    std::unordered_map<std::string, std::pair<std::string, VirtualMachine*>> mounts;
};

} // namespace multipass
#endif // MULTIPASS_QEMU_MOUNT_HANDLER_H
