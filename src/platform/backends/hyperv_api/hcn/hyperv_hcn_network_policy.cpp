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

#include <hyperv_api/hcn/hyperv_hcn_network_policy.h>
#include <hyperv_api/hcn/hyperv_hcn_network_policy_netadaptername.h>
#include <hyperv_api/hyperv_api_string_conversion.h>

namespace hv = multipass::hyperv;
namespace hcn = hv::hcn;

template <typename Char>
struct NetworkPolicySettingsFormatters
{
    auto operator()(const hcn::HcnNetworkPolicyNetAdapterName& policy)
    {
        constexpr static auto netadaptername_settings_template = MULTIPASS_UNIVERSAL_LITERAL(R"json(
            "NetworkAdapterName": "{}"
        )json");

        return fmt::format(netadaptername_settings_template.as<Char>(), hv::maybe_widen{policy.net_adapter_name});
    }
};

template <typename Char>
template <typename FormatContext>
auto fmt::formatter<hcn::HcnNetworkPolicy, Char>::format(const hcn::HcnNetworkPolicy& policy, FormatContext& ctx) const
    -> typename FormatContext::iterator
{
    constexpr static auto route_template = MULTIPASS_UNIVERSAL_LITERAL(R"json(
        {{
            "Type": "{}",
            "Settings": {{
                {}
            }}
        }}
    )json");

    return format_to(ctx.out(),
                     route_template.as<Char>(),
                     hv::maybe_widen{policy.type},
                     std::visit(NetworkPolicySettingsFormatters<Char>{}, policy.settings));
}

template auto fmt::formatter<hcn::HcnNetworkPolicy, char>::format<fmt::format_context>(const hcn::HcnNetworkPolicy&,
                                                                                       fmt::format_context&) const
    -> fmt::format_context::iterator;

template auto fmt::formatter<hcn::HcnNetworkPolicy, wchar_t>::format<fmt::wformat_context>(const hcn::HcnNetworkPolicy&,
                                                                                           fmt::wformat_context&) const
    -> fmt::wformat_context::iterator;
