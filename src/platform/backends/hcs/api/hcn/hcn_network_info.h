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

#include <hcs/api/hcn/hcn_dns.h>

#include <fmt/xchar.h>

#include <optional>
#include <string>

namespace multipass::hyperv::hcn
{
struct HcnNetworkInfo
{
    std::string guid;
    std::string name;
    std::string type;
    std::optional<std::string> network_adapter_name;
    // Interface GUID of the network's host vNIC (e.g. "vEthernet (Default Switch)"), if any.
    std::optional<std::string> host_interface_guid;

    /**
     * DNS settings (suffix, search list, servers) reported by the network.
     */
    std::optional<HcnDns> dns;
};

}; // namespace multipass::hyperv::hcn

/**
 * Formatter type specialization for HcnNetworkInfo
 */
template <typename Char>
struct fmt::formatter<multipass::hyperv::hcn::HcnNetworkInfo, Char>
    : formatter<basic_string_view<Char>, Char>
{
    template <typename FormatContext>
    auto format(const multipass::hyperv::hcn::HcnNetworkInfo& info, FormatContext& ctx) const
        -> FormatContext::iterator;
};
