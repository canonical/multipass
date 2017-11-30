/*
 * Copyright (C) 2017 Canonical, Ltd.
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
 * Authored by: Alberto Aguirre <alberto.aguirre@canonical.com>
 *
 */

#include <multipass/platform.h>
#include <multipass/virtual_machine_factory.h>

#include "backends/qemu/qemu_virtual_machine_factory.h"

namespace mp = multipass;

// TODO: runtime platform checks?

std::string mp::Platform::default_server_address()
{
    return {"localhost:50051"};
}

mp::VirtualMachineFactory::UPtr mp::Platform::vm_backend(const mp::Path& data_dir)
{
    return std::make_unique<QemuVirtualMachineFactory>(data_dir);
}
