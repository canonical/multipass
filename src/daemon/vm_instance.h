/*
 * Copyright (C) 2019 Canonical, Ltd.
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

#ifndef MULTIPASS_VM_INSTANCE_H
#define MULTIPASS_VM_INSTANCE_H

#include <multipass/virtual_machine.h>
#include <multipass/ssh/ssh_key_provider.h>

namespace multipass
{

class VMInstance
{
public:
    using UPtr = std::unique_ptr<VMInstance>;

    VMInstance(const VirtualMachine::ShPtr vm, const SSHKeyProvider &key_provider);

    const VirtualMachine::ShPtr vm() const;

    std::string run_command(const std::string& cmd);

private:
    VMInstance(const VMInstance&) = delete;
    VMInstance& operator=(const VMInstance&) = delete;
    VMInstance(VirtualMachine::ShPtr vm, const SSHKeyProvider &&) = delete; // avoid accidentally accepting temporary

    const VirtualMachine::ShPtr vm_shptr;
    const SSHKeyProvider &key_provider;
};

} // namespace multipass
#endif // MULTIPASS_VM_INSTANCE_H
