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

#include <apple/apple_virtual_machine.h>

#include <multipass/exceptions/virtual_machine_state_exceptions.h>
#include <multipass/top_catch_all.h>
#include <multipass/vm_status_monitor.h>

namespace multipass::apple
{
AppleVZVirtualMachine::AppleVZVirtualMachine(const VirtualMachineDescription& desc,
                                             VMStatusMonitor& monitor,
                                             const SSHKeyProvider& key_provider,
                                             const Path& instance_dir)
    : BaseVirtualMachine{desc.vm_name, key_provider, instance_dir}, desc{desc}, monitor{&monitor}
{
}

AppleVZVirtualMachine::~AppleVZVirtualMachine()
{
}

void AppleVZVirtualMachine::start()
{
}

void AppleVZVirtualMachine::shutdown(ShutdownPolicy shutdown_policy)
{
}

void AppleVZVirtualMachine::suspend()
{
}
} // namespace multipass::apple
