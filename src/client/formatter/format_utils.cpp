/*
 * Copyright (C) 2018 Canonical, Ltd.
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

#include <multipass/cli/format_utils.h>

namespace mp = multipass;

std::string mp::format::status_string_for(const mp::InstanceStatus& status)
{
    std::string status_val;

    switch (status.status())
    {
    case mp::InstanceStatus::RUNNING:
        status_val = "RUNNING";
        break;
    case mp::InstanceStatus::STOPPED:
        status_val = "STOPPED";
        break;
    case mp::InstanceStatus::DELETED:
        status_val = "DELETED";
        break;
    case mp::InstanceStatus::STARTING:
        status_val = "STARTING";
        break;
    case mp::InstanceStatus::RESTARTING:
        status_val = "RESTARTING";
        break;
    case mp::InstanceStatus::DELAYED_SHUTDOWN:
        status_val = "DELAYED SHUTDOWN";
        break;
    default:
        status_val = "UNKNOWN";
        break;
    }
    return status_val;
}
