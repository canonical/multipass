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

#include "qemu_virtual_machine.h"

#include <multipass/mount_handler.h>

namespace multipass
{
class QemuMountHandler : public MountHandler
{
public:
    QemuMountHandler(QemuVirtualMachine* vm,
                     const SSHKeyProvider* ssh_key_provider,
                     const std::string& target,
                     VMMount mount_spec);
    ~QemuMountHandler() override;

    void activate_impl(ServerVariant server, std::chrono::milliseconds timeout) override;
    void deactivate_impl(bool force) override;
    bool is_active() override;

private:
    QemuVirtualMachine::MountArgs& vm_mount_args;
    std::string tag;
};

} // namespace multipass
