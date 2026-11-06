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

#include "command_messages.h"

#include <fmt/core.h>
#include <string>

namespace multipass
{

struct ImagesUserMessages
{
    static std::string search_remote_hint(const std::string& remote_name)
    {
        return fmt::format("To search a specific remote, use ‘multipass images {}:’\n"
                           "Run ‘multipass images --remotes’ to see all available remotes.\n",
                           remote_name);
    }

    static std::string remote_not_found(const std::string& remote_name)
    {
        return fmt::format("Remote \'{}\' is not found. "
                           "Please use ‘multipass images --remotes’ for supported remotes.",
                           remote_name);
    }
};

} // namespace multipass
