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

#include "backends/hyperkit/hyperkit_virtual_machine_factory.h"

#include <set>

namespace mp = multipass;

namespace
{
const std::set<std::string> supported_aliases{"default", "lts", "16.04", "x", "xenial", "b", "bionic", "18.04"};
}

std::string mp::platform::default_server_address()
{
    return {"localhost:50051"};
}

mp::VirtualMachineFactory::UPtr mp::platform::vm_backend(const mp::Path& data_dir)
{
    return std::make_unique<HyperkitVirtualMachineFactory>();
}

mp::logging::Logger::UPtr mp::platform::make_logger(mp::logging::Level level)
{
    return nullptr;
}

bool mp::platform::is_alias_supported(const std::string& alias)
{
    if (qgetenv("MULTIPASS_UNLOCK") == "lucky-chimp")
        return true;

    if (supported_aliases.find(alias) != supported_aliases.end())
    {
        return true;
    }

    return false;
}
