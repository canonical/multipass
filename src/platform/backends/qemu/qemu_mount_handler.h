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

#include "qemu_virtual_machine.h"

#include <multipass/mount_handler.h>

namespace multipass
{
class QemuMountHandler : public MountHandler
{
public:
    QemuMountHandler(QemuVirtualMachine* vm, const SSHKeyProvider* ssh_key_provider, const std::string& target,
                     const VMMount& mount);
    ~QemuMountHandler() override;

    void start_impl(ServerVariant server, std::chrono::milliseconds timeout = std::chrono::minutes(5)) override;
    void stop_impl() override;

private:
    QemuVirtualMachine::MountArgs& vm_mount_args;
    std::string tag;
};

} // namespace multipass
#endif // MULTIPASS_QEMU_MOUNT_HANDLER_H
