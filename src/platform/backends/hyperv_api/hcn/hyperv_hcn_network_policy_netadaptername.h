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

#ifndef MULTIPASS_HYPERV_API_HCN_NETWORK_POLICY_NETADAPTERNAME_H
#define MULTIPASS_HYPERV_API_HCN_NETWORK_POLICY_NETADAPTERNAME_H

#include <fmt/format.h>

#include <string>

namespace multipass::hyperv::hcn
{

struct HcnNetworkPolicyNetAdapterName
{
    std::string net_adapter_name;
};

} // namespace multipass::hyperv::hcn

/**
 * Formatter type specialization for HcnNetworkPolicyNetAdapterName
 */
template <typename Char>
struct fmt::formatter<multipass::hyperv::hcn::HcnNetworkPolicyNetAdapterName, Char>
{
    constexpr auto parse(basic_format_parse_context<Char>& ctx)
    {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const multipass::hyperv::hcn::HcnNetworkPolicyNetAdapterName& policy, FormatContext& ctx) const
    {
        return format_to(ctx.out(), "NetAdapterName: ({})", policy.net_adapter_name);
    }
};

#endif
