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

#include "migrating_hyperv_virtual_machine.h"

#include <cassert>

multipass::hyperv::MigratingHyperVVirtualMachine::MigratingHyperVVirtualMachine(
    VirtualMachine::UPtr legacy_vm,
    std::unique_ptr<HyperVMigrator> migrator)
    : DelegatingVirtualMachine{std::move(legacy_vm)}, migrator{std::move(migrator)}
{
    assert(this->migrator);
}

void multipass::hyperv::MigratingHyperVVirtualMachine::switch_to_target()
{
    clear_delegate();
    try
    {
        replace_delegate(migrator->make_target());
    }
    catch (...)
    {
        state = State::unknown;
        throw;
    }
}

void multipass::hyperv::MigratingHyperVVirtualMachine::start()
{
    const std::lock_guard lock{migration_mutex};
    auto vm = delegate_or_null();
    if (!vm)
    {
        assert(migration_committed);
        switch_to_target();
    }
    else if (!migration_committed)
    {
        const auto current = vm->current_state();
        if ((current == State::off || current == State::stopped) &&
            migrator->try_migrate(*vm))
        {
            migration_committed = true;
            switch_to_target();
        }
    }

    DelegatingVirtualMachine::start();
}
