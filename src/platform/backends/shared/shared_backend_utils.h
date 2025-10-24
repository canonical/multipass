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

#include "base_virtual_machine.h"

#include <multipass/exceptions/start_exception.h>

#include <chrono>
#include <string>

namespace multipass
{
namespace backend
{
using namespace std::chrono_literals;

template <typename Callable>
void ensure_vm_is_running_for(BaseVirtualMachine* virtual_machine,
                              Callable&& is_vm_running,
                              const std::string& msg)
{
    std::lock_guard lock{virtual_machine->state_mutex};
    if (!is_vm_running())
    {
        virtual_machine->shutdown_while_starting = true;
        virtual_machine->state_wait.notify_all();
        throw StartException(virtual_machine->vm_name, msg);
    }
}
} // namespace backend
} // namespace multipass
