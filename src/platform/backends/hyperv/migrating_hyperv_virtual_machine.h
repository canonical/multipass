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

#include "delegating_virtual_machine.h"
#include "hyperv_migrator.h"

namespace multipass::hyperv
{
class MigratingHyperVVirtualMachine final : public DelegatingVirtualMachine
{
public:
    MigratingHyperVVirtualMachine(VirtualMachine::UPtr legacy_vm,
                                  std::unique_ptr<HyperVMigrator> migrator);

    void start() override;

private:
    void switch_to_target();

    std::unique_ptr<HyperVMigrator> migrator;
    std::mutex migration_mutex;
    bool migration_committed{false};
};
} // namespace multipass::hyperv
