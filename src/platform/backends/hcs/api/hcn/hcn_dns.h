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

#include <fmt/format.h>
#include <fmt/xchar.h>

#include <string>
#include <vector>

namespace multipass::hyperv::hcn
{

/**
 * Domain Name System settings associated with a Host Compute Network.
 *
 * Maps to the `Dns` object of the HCN `HostComputeNetwork` schema.
 */
struct HcnDns
{
    /**
     * Primary DNS suffix (domain) for the network.
     */
    std::string domain{};

    /**
     * DNS suffix search list.
     */
    std::vector<std::string> search{};

    /**
     * List of DNS server IP addresses.
     */
    std::vector<std::string> server_list{};

    /**
     * Additional DNS options.
     */
    std::vector<std::string> options{};
};

} // namespace multipass::hyperv::hcn

/**
 * Formatter type specialization for HcnDns
 */
template <typename Char>
struct fmt::formatter<multipass::hyperv::hcn::HcnDns, Char>
    : formatter<basic_string_view<Char>, Char>
{
    template <typename FormatContext>
    auto format(const multipass::hyperv::hcn::HcnDns& dns, FormatContext& ctx) const
        -> FormatContext::iterator;
};
