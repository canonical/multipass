/*
 * Copyright (C) 2017-2018 Canonical, Ltd.
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

#include "backends/libvirt/libvirt_virtual_machine_factory.h"
#include "backends/qemu/qemu_virtual_machine_factory.h"
#include "logger/journald_logger.h"

namespace mp = multipass;

std::string mp::platform::default_server_address()
{
    return {"unix:/run/multipass_socket"};
}

mp::VirtualMachineFactory::UPtr mp::platform::vm_backend(const mp::Path& data_dir)
{
    auto backend = qgetenv("MULTIPASS_VM_BACKEND");

    if (backend.isEmpty() || backend == "QEMU")
        return std::make_unique<QemuVirtualMachineFactory>(data_dir);
    else if (backend == "LIBVIRT")
        return std::make_unique<LibVirtVirtualMachineFactory>(data_dir);

    throw std::runtime_error("Invalid VM backend set in the environment");
}

mp::logging::Logger::UPtr mp::platform::make_logger(mp::logging::Level level)
{
    return std::make_unique<logging::JournaldLogger>(level);
}
