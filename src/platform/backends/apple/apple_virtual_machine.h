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

#include <apple/apple_vz_wrapper.h>

#include <multipass/virtual_machine_description.h>
#include <shared/base_virtual_machine.h>

namespace multipass
{
class VMStatusMonitor;
}

namespace multipass::apple
{
class AppleVZVirtualMachine : public BaseVirtualMachine
{
public:
    AppleVZVirtualMachine(const VirtualMachineDescription& desc,
                          VMStatusMonitor& monitor,
                          const SSHKeyProvider& key_provider,
                          const Path& instance_dir);
    ~AppleVZVirtualMachine();

    void start() override;
    void shutdown(ShutdownPolicy shutdown_policy = ShutdownPolicy::Powerdown) override;
    void suspend() override;

private:
    VirtualMachineDescription desc;
    VMStatusMonitor* monitor;
    VMHandle vm_handle{nullptr};
};
} // namespace multipass::apple
