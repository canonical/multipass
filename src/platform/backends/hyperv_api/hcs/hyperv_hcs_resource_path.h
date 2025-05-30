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

#ifndef MULTIPASS_HYPERV_API_HCS_RESOURCE_PATH_H
#define MULTIPASS_HYPERV_API_HCS_RESOURCE_PATH_H

#include <fmt/format.h>

#include <string>
#include <string_view>

namespace multipass::hyperv::hcs
{

struct HcsResourcePath
{
    operator std::string_view() const
    {
        return value;
    }

    operator const std::string&() const
    {
        return value;
    }

    static HcsResourcePath NetworkAdapters(const std::string& network_adapter_id)
    {
        return fmt::format("VirtualMachine/Devices/NetworkAdapters/{{{0}}}", network_adapter_id);
    }

    static HcsResourcePath Memory()
    {
        return std::string{"VirtualMachine/ComputeTopology/Memory/SizeInMB"};
    }

    static HcsResourcePath Plan9Shares()
    {
        return std::string{"VirtualMachine/Devices/Plan9/Shares"};
    }

private:
    HcsResourcePath(std::string v) : value(v)
    {
    }

    std::string value{};
};

} // namespace multipass::hyperv::hcs

#endif
