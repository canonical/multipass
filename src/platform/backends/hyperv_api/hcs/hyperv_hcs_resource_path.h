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

#include <hyperv_api/format_as_mixin.h>

#include <string_view>

#include <fmt/format.h>

namespace multipass::hyperv::hcs
{

struct HcsResourcePath : FormatAsMixin<HcsResourcePath>
{
    [[nodiscard]] operator std::string_view() const
    {
        return value;
    }

    [[nodiscard]] static HcsResourcePath NetworkAdapters(const std::string& network_adapter_id)
    {
        return fmt::format("VirtualMachine/Devices/NetworkAdapters/{{{0}}}", network_adapter_id);
    }

    [[nodiscard]] static HcsResourcePath Memory()
    {
        return {"VirtualMachine/ComputeTopology/Memory/SizeInMB"};
    }

    [[nodiscard]] static HcsResourcePath Plan9Shares()
    {
        return {"VirtualMachine/Devices/Plan9/Shares"};
    }

    [[nodiscard]] bool operator==(const HcsResourcePath& rhs) const
    {
        return value == rhs.value;
    }

private:
    HcsResourcePath(std::string v) : value(std::move(v))
    {
    }

    std::string value{};
};

} // namespace multipass::hyperv::hcs
