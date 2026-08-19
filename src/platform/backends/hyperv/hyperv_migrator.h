/*
 * Copyright (C) Canonical, Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "hyperv_migration_state.h"

#include <multipass/path.h>
#include <multipass/virtual_machine.h>
#include <multipass/virtual_machine_description.h>

#include <memory>
#include <optional>

namespace multipass
{
class AvailabilityZone;
class SSHKeyProvider;
class VMStatusMonitor;
}

namespace multipass::hyperv
{
class HyperVMigrator
{
public:
    virtual ~HyperVMigrator() = default;

    [[nodiscard]] virtual bool migrate(VirtualMachine& legacy_vm) = 0;
    [[nodiscard]] virtual VirtualMachine::UPtr make_target() = 0;
};

class DefaultHyperVMigrator final : public HyperVMigrator
{
public:
    DefaultHyperVMigrator(VirtualMachineDescription description,
                          VMStatusMonitor& monitor,
                          const SSHKeyProvider& key_provider,
                          AvailabilityZone& zone,
                          Path instance_dir);

    [[nodiscard]] bool migrate(VirtualMachine& legacy_vm) override;
    [[nodiscard]] VirtualMachine::UPtr make_target() override;

private:
    VirtualMachineDescription description;
    VMStatusMonitor& monitor;
    const SSHKeyProvider& key_provider;
    AvailabilityZone& zone;
    Path instance_dir;
    std::optional<HyperVMigrationState> committed_state;
};
} // namespace multipass::hyperv
