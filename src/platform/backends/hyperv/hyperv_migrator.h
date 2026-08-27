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

#include "hcs_ownership.h"

#include <multipass/path.h>
#include <multipass/virtual_machine.h>
#include <multipass/virtual_machine_description.h>

#include <filesystem>
#include <memory>
#include <optional>

namespace multipass
{
class AvailabilityZone;
class SSHKeyProvider;
class VMStatusMonitor;
} // namespace multipass

namespace multipass::hyperv
{
struct LegacyDiskLayout;

class HyperVMigrator
{
public:
    virtual ~HyperVMigrator() = default;

    // Returns true only after the migration crosses the commit boundary.
    [[nodiscard]] virtual bool try_migrate(VirtualMachine& legacy_vm) = 0;
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

    [[nodiscard]] bool try_migrate(VirtualMachine& legacy_vm) override;
    [[nodiscard]] VirtualMachine::UPtr make_target() override;

private:
    void commit_migration(const LegacyDiskLayout& layout, HCSOwnership ownership);
    VirtualMachineDescription description;
    VMStatusMonitor& monitor;
    const SSHKeyProvider& key_provider;
    AvailabilityZone& zone;
    Path instance_dir;
    std::optional<HCSOwnership> committed_ownership;
};
} // namespace multipass::hyperv
