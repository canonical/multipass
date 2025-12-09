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
#include <hyperv_api/hcs/hyperv_hcs_network_adapter.h>

#include <hyperv_api/hyperv_api_string_conversion.h>

using multipass::hyperv::maybe_widen;
using multipass::hyperv::hcs::HcsNetworkAdapter;

template <typename Char>
template <typename FormatContext>
auto fmt::formatter<HcsNetworkAdapter, Char>::format(const HcsNetworkAdapter& network_adapter,
                                                     FormatContext& ctx) const
    -> FormatContext::iterator
{
    constexpr static auto network_adapter_template = MULTIPASS_UNIVERSAL_LITERAL(R"json(
        "{0}": {{
            "EndpointId" : "{0}",
            "MacAddress": "{1}",
            "InstanceId": "{0}"
        }}
    )json");

    return fmt::format_to(ctx.out(),
                          network_adapter_template.as<Char>(),
                          maybe_widen{network_adapter.endpoint_guid},
                          maybe_widen{network_adapter.mac_address});
}

template auto fmt::formatter<HcsNetworkAdapter, char>::format<fmt::format_context>(
    const HcsNetworkAdapter&,
    fmt::format_context&) const -> fmt::format_context::iterator;

template auto fmt::formatter<HcsNetworkAdapter, wchar_t>::format<fmt::wformat_context>(
    const HcsNetworkAdapter&,
    fmt::wformat_context&) const -> fmt::wformat_context::iterator;
