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

#include <string_view>

namespace multipass::hyperv::hcn
{

/**
 * Strongly-typed string values for network type.
 */
struct HcnNetworkType
{
    [[nodiscard]] operator std::string_view() const
    {
        return value;
    }

    /**
     * @since Version 2.0
     */
    [[nodiscard]] static HcnNetworkType Nat()
    {
        return {"NAT"};
    }

    /**
     * @since Version 2.0
     */
    [[nodiscard]] static HcnNetworkType Ics()
    {
        return {"ICS"};
    }

    /**
     * @since Version 2.0
     */
    [[nodiscard]] static HcnNetworkType Transparent()
    {
        return {"Transparent"};
    }

    /**
     * @since Version 2.0
     */
    [[nodiscard]] static HcnNetworkType L2Bridge()
    {
        return {"L2Bridge"};
    }

    [[nodiscard]] bool operator==(const HcnNetworkType& rhs) const
    {
        return value == rhs.value;
    }

private:
    constexpr HcnNetworkType(std::string_view v) : value(v)
    {
    }

    std::string_view value{};
};

} // namespace multipass::hyperv::hcn
