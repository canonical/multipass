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

#include "backends/qemu/qemu_virtual_machine_factory.h"

#include <unistd.h>

namespace mp = multipass;

std::string mp::platform::default_server_address()
{
    return {"unix:/run/multipass_socket"};
}

mp::VirtualMachineFactory::UPtr mp::platform::vm_backend(const mp::Path& data_dir)
{
    return std::make_unique<QemuVirtualMachineFactory>(data_dir);
}

int mp::platform::chown(const char* path, unsigned int uid, unsigned int gid)
{
    return ::chown(path, uid, gid);
}

bool mp::platform::symlink(const char* target, const char* link, bool is_dir)
{
    return ::symlink(target, link) == 0;
}
