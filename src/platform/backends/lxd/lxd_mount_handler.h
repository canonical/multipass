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

#ifndef MULTIPASS_LXD_MOUNT_HANDLER_H
#define MULTIPASS_LXD_MOUNT_HANDLER_H

#include "lxd_virtual_machine.h"
#include "multipass/mount_handler.h"

namespace multipass
{
class LXDMountHandler : public MountHandler
{
public:
    LXDMountHandler(NetworkAccessManager* network_manager,
                    LXDVirtualMachine* lxd_virtual_machine,
                    const SSHKeyProvider* ssh_key_provider,
                    const std::string& target_path,
                    VMMount mount_spec);
    ~LXDMountHandler() override;

    void activate_impl(ServerVariant server, std::chrono::milliseconds timeout) override;
    void deactivate_impl(bool force) override;

    bool is_mount_managed_by_backend() override
    {
        return true;
    }

private:
    void lxd_device_add();
    void lxd_device_remove();

    // data member
    NetworkAccessManager* network_manager{nullptr};
    const QUrl lxd_instance_endpoint{};
    const std::string device_name{};
};

} // namespace multipass
#endif // MULTIPASS_LXD_MOUNT_HANDLER_H
