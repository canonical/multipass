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

#ifndef MULTIPASS_HYPERV_API_HCN_NETWORK_POLICY_H
#define MULTIPASS_HYPERV_API_HCN_NETWORK_POLICY_H

#include <hyperv_api/hcn/hyperv_hcn_network_policy_netadaptername.h>
#include <hyperv_api/hcn/hyperv_hcn_network_policy_type.h>

#include <fmt/xchar.h>

#include <variant>

namespace multipass::hyperv::hcn
{

struct HcnNetworkPolicy
{
    /**
     * The type of the network policy.
     */
    HcnNetworkPolicyType type;

    /**
     * Right now, there's only one policy type defined but
     * it might expand in the future, so let's go an extra
     * mile to future-proof this code.
     */
    std::variant<HcnNetworkPolicyNetAdapterName> settings;
};

} // namespace multipass::hyperv::hcn

/**
 * Formatter type specialization for HcnNetworkPolicy
 */
template <typename Char>
struct fmt::formatter<multipass::hyperv::hcn::HcnNetworkPolicy, Char> : formatter<basic_string_view<Char>, Char>
{
    template <typename FormatContext>
    auto format(const multipass::hyperv::hcn::HcnNetworkPolicy& policy, FormatContext& ctx) const ->
        typename FormatContext::iterator;
};

#endif
