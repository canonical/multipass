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

#include <string>
#include <string_view>

namespace multipass::hyperv::hcn
{

/**
 * Strongly-typed string values for
 * network policy types.
 *
 * @ref
 * https://github.com/MicrosoftDocs/Virtualization-Documentation/blob/51b2c0024ce9fc0c9c240fe8e14b170e05c57099/virtualization/api/hcn/HNS_Schema.md?plain=1#L522
 */
struct HcnNetworkPolicyType
{
    operator std::string_view() const
    {
        return value;
    }

    operator std::string() const
    {
        return std::string{value};
    }

    /**
     * @since Version 2.0
     */
    constexpr static auto SourceMacAddress()
    {
        return HcnNetworkPolicyType{"SourceMacAddress"};
    }

    /**
     * @since Version 2.0
     */
    constexpr static auto NetAdapterName()
    {
        return HcnNetworkPolicyType{"NetAdapterName"};
    }

    /**
     * @since Version 2.0
     */
    constexpr static auto VSwitchExtension()
    {
        return HcnNetworkPolicyType{"VSwitchExtension"};
    }

    /**
     * @since Version 2.0
     */
    constexpr static auto DrMacAddress()
    {
        return HcnNetworkPolicyType{"DrMacAddress"};
    }

    /**
     * @since Version 2.0
     */
    constexpr static auto AutomaticDNS()
    {
        return HcnNetworkPolicyType{"AutomaticDNS"};
    }

    /**
     * @since Version 2.0
     */
    constexpr static auto InterfaceConstraint()
    {
        return HcnNetworkPolicyType{"InterfaceConstraint"};
    }

    /**
     * @since Version 2.0
     */
    constexpr static auto ProviderAddress()
    {
        return HcnNetworkPolicyType{"ProviderAddress"};
    }

    /**
     * @since Version 2.0
     */
    constexpr static auto RemoteSubnetRoute()
    {
        return HcnNetworkPolicyType{"RemoteSubnetRoute"};
    }

    /**
     * @since Version 2.0
     */
    constexpr static auto VxlanPort()
    {
        return HcnNetworkPolicyType{"VxlanPort"};
    }

    /**
     * @since Version 2.0
     */
    constexpr static auto HostRoute()
    {
        return HcnNetworkPolicyType{"HostRoute"};
    }

    /**
     * @since Version 2.0
     */
    constexpr static auto SetPolicy()
    {
        return HcnNetworkPolicyType{"SetPolicy"};
    }

    /**
     * @since Version 2.0
     */
    constexpr static auto NetworkL4Proxy()
    {
        return HcnNetworkPolicyType{"NetworkL4Proxy"};
    }

    /**
     * @since Version 2.0
     */
    constexpr static auto LayerConstraint()
    {
        return HcnNetworkPolicyType{"LayerConstraint"};
    }

    /**
     * @since Version 2.0
     */
    constexpr static auto NetworkACL()
    {
        return HcnNetworkPolicyType{"NetworkACL"};
    }

private:
    constexpr HcnNetworkPolicyType(std::string_view v) : value(v)
    {
    }

    std::string_view value{};
};

} // namespace multipass::hyperv::hcn
