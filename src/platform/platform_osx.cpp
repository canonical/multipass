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

#include <unordered_map>
#include <unordered_set>

#include <QFileInfo>
#include <QString>

#include <unistd.h>

namespace mp = multipass;

namespace
{
constexpr auto unlock_code{"glorious-toad"};
const std::unordered_set<std::string> supported_release_aliases{"default", "lts", "16.04",  "x",
                                                                "xenial",  "b",   "bionic", "18.04"};
const std::unordered_set<std::string> supported_snapcraft_aliases{"core", "core16", "core18"};
const std::unordered_map<std::string, std::unordered_set<std::string>> supported_remotes_aliases_map{
    {"release", supported_release_aliases}, {"snapcraft", supported_snapcraft_aliases}};
}

std::string mp::platform::default_server_address()
{
    return {"unix:/var/run/multipass_socket"};
}

mp::VirtualMachineFactory::UPtr mp::platform::vm_backend(const mp::Path& data_dir)
{
    return std::make_unique<HyperkitVirtualMachineFactory>();
}

mp::logging::Logger::UPtr mp::platform::make_logger(mp::logging::Level level)
{
    return nullptr;
}

bool mp::platform::link(const char* target, const char* link)
{
    QFileInfo file_info{target};

    if (file_info.isSymLink())
    {
        return ::symlink(file_info.symLinkTarget().toStdString().c_str(), link) == 0;
    }

    return ::link(target, link) == 0;
}

bool mp::platform::is_alias_supported(const std::string& alias, const std::string& remote)
{
    if (remote.empty() && alias == "core")
        return false;

    if (qgetenv("MULTIPASS_UNLOCK") == unlock_code)
        return true;

    if (remote.empty())
    {
        if (supported_release_aliases.find(alias) != supported_release_aliases.end())
            return true;
    }
    else
    {
        auto it = supported_remotes_aliases_map.find(remote);

        if (it != supported_remotes_aliases_map.end())
        {
            if (it->second.find(alias) != it->second.end())
                return true;
        }
    }

    return false;
}

bool mp::platform::is_remote_supported(const std::string& remote)
{
    if (remote.empty() || qgetenv("MULTIPASS_UNLOCK") == unlock_code)
        return true;

    if (supported_remotes_aliases_map.find(remote) != supported_remotes_aliases_map.end())
    {
        return true;
    }

    return false;
}

bool mp::platform::is_image_url_supported()
{
    if (qgetenv("MULTIPASS_UNLOCK") == unlock_code)
        return true;

    return false;
}
