/*
 * Copyright (C) 2017-2019 Canonical, Ltd.
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

#include <multipass/platform.h>
#include <multipass/virtual_machine_factory.h>

#include "backends/hyperkit/hyperkit_virtual_machine_factory.h"
#include "platform_proprietary.h"

#include <QFileInfo>
#include <QString>

#include <unistd.h>

namespace mp = multipass;

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

    if (check_unlock_code())
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
    if (remote.empty() || check_unlock_code())
        return true;

    if (supported_remotes_aliases_map.find(remote) != supported_remotes_aliases_map.end())
    {
        return true;
    }

    return false;
}

bool mp::platform::is_image_url_supported()
{
    return check_unlock_code();
}
