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

using namespace multipass::hyperv;
using namespace multipass::hyperv::hcn;
using namespace multipass::hyperv::literals;

template <typename Char>
struct NetworkPolicySettingsFormatters
{
    auto operator()(const HcnNetworkPolicyNetAdapterName& policy) const
    {
        static constexpr auto netadaptername_settings_template = R"json(
            "NetworkAdapterName": "{}"
        )json"_unv;

        return fmt::format(netadaptername_settings_template.as<Char>(),
                           maybe_widen{policy.net_adapter_name});
    }
};

template <typename Char>
template <typename FormatContext>
auto fmt::formatter<HcnNetworkPolicy, Char>::format(const HcnNetworkPolicy& policy,
                                                    FormatContext& ctx) const
    -> FormatContext::iterator
{
    static constexpr auto route_template = R"json(
        {{
            "Type": "{}",
            "Settings": {{
                {}
            }}
        }}
    )json"_unv;

    return fmt::format_to(ctx.out(),
                          route_template.as<Char>(),
                          maybe_widen{policy.type},
                          std::visit(NetworkPolicySettingsFormatters<Char>{}, policy.settings));
}

template auto fmt::formatter<HcnNetworkPolicy, char>::format<fmt::format_context>(
    const HcnNetworkPolicy&,
    fmt::format_context&) const -> fmt::format_context::iterator;

template auto fmt::formatter<HcnNetworkPolicy, wchar_t>::format<fmt::wformat_context>(
    const HcnNetworkPolicy&,
    fmt::wformat_context&) const -> fmt::wformat_context::iterator;
