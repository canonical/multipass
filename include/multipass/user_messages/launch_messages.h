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

#include <multipass/rpc/multipass.pb.h>

#include <string_view>
#include <vector>

namespace multipass
{

struct LaunchUserMessages
{
    struct MountRoute
    {
        std::string source;
        std::string target;
    };

    using MountRoutes = std::vector<MountRoute>;

    static std::string client_timeout(std::string_view vm_name, const MountRoutes& mount_routes);
    static std::string server_timeout(ReadinessError::ErrorCode readiness_error,
                                      std::string_view vm_name,
                                      const MountRoutes& mount_routes);
};

} // namespace multipass
