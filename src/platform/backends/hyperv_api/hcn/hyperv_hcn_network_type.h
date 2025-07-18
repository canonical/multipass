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
 * Strongly-typed string values for
 * network type.
 */
struct HcnNetworkType
{
    operator std::string_view() const
    {
        return value;
    }

    /**
     * @since Version 2.0
     */
    constexpr static auto Nat()
    {
        return HcnNetworkType{"NAT"};
    }

    /**
     * @since Version 2.0
     */
    constexpr static auto Ics()
    {
        return HcnNetworkType{"ICS"};
    }

    /**
     * @since Version 2.0
     */
    constexpr static auto Transparent()
    {
        return HcnNetworkType{"Transparent"};
    }

    /**
     * @since Version 2.0
     */
    constexpr static auto L2Bridge()
    {
        return HcnNetworkType{"L2Bridge"};
    }

    /**
     * @since Version 2.0
     */
    constexpr static auto L2Tunnel()
    {
        return HcnNetworkType{"L2Tunnel"};
    }

    /**
     * @since Version 2.0
     */
    constexpr static auto Overlay()
    {
        return HcnNetworkType{"Overlay"};
    }

    /**
     * @since Version 2.0
     */
    constexpr static auto Private()
    {
        return HcnNetworkType{"Private"};
    }

    /**
     * @since Version 2.0
     */
    constexpr static auto Internal()
    {
        return HcnNetworkType{"Internal"};
    }

    /**
     * @since Version 2.4
     */
    constexpr static auto Mirrored()
    {
        return HcnNetworkType{"Mirrored"};
    }

    /**
     * @since Version 2.4
     */
    constexpr static auto Infiniband()
    {
        return HcnNetworkType{"Infiniband"};
    }

    /**
     * @since Version 2.10
     */
    constexpr static auto ConstrainedICS()
    {
        return HcnNetworkType{"ConstrainedICS"};
    }

private:
    constexpr HcnNetworkType(std::string_view v) : value(v)
    {
    }

    std::string_view value{};
};

} // namespace multipass::hyperv::hcn