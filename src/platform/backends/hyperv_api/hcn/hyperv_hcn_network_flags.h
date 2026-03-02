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
#include <fmt/ranges.h>

#include <cstdint>
#include <string_view>
#include <vector>

namespace multipass::hyperv::hcn
{

/**
 * https://github.com/MicrosoftDocs/Virtualization-Documentation/blob/51b2c0024ce9fc0c9c240fe8e14b170e05c57099/virtualization/api/hcn/HNS_Schema.md?plain=1#L486
 */
enum class HcnNetworkFlags : std::uint32_t
{
    none = 0,                       ///< 2.0
    enable_dns_proxy = 1 << 0,      ///< 2.0
    enable_dhcp_server = 1 << 1,    ///< 2.0
    enable_mirroring = 1 << 2,      ///< 2.0
    enable_non_persistent = 1 << 3, ///< 2.0
    isolate_vswitch = 1 << 4,       ///< 2.0
    enable_flow_steering = 1 << 5,  ///< 2.11
    disable_sharing = 1 << 6,       ///< 2.14
    enable_firewall = 1 << 7,       ///< 2.14
    disable_host_port = 1 << 10,    ///< ??
    enable_iov = 1 << 13,           ///< ??
};

[[nodiscard]] inline HcnNetworkFlags operator|(HcnNetworkFlags lhs, HcnNetworkFlags rhs) noexcept
{
    using U = std::underlying_type_t<HcnNetworkFlags>;
    return static_cast<HcnNetworkFlags>(static_cast<U>(lhs) | static_cast<U>(rhs));
}

inline HcnNetworkFlags& operator|=(HcnNetworkFlags& lhs, HcnNetworkFlags rhs) noexcept
{
    using U = std::underlying_type_t<HcnNetworkFlags>;
    lhs = (lhs | rhs);
    return lhs;
}

} // namespace multipass::hyperv::hcn

/**
 *  Formatter type specialization for HcnNetworkFlags
 */
template <typename Char>
struct fmt::formatter<multipass::hyperv::hcn::HcnNetworkFlags, Char>
    : formatter<basic_string_view<Char>, Char>
{
    template <typename FormatContext>
    auto format(multipass::hyperv::hcn::HcnNetworkFlags flags, FormatContext& ctx) const
    {
        std::vector<std::string_view> parts;

        auto is_flag_set = [](decltype(flags) flags, decltype(flags) flag) {
            const auto flags_u = fmt::underlying(flags);
            const auto flag_u = fmt::underlying(flag);
            return flags_u & flag_u;
        };

        if (flags == decltype(flags)::none)
        {
            parts.emplace_back("none");
        }
        else
        {
            if (is_flag_set(flags, decltype(flags)::enable_dns_proxy))
                parts.emplace_back("enable_dns_proxy");
            if (is_flag_set(flags, decltype(flags)::enable_dhcp_server))
                parts.emplace_back("enable_dhcp_server");
            if (is_flag_set(flags, decltype(flags)::enable_mirroring))
                parts.emplace_back("enable_mirroring");
            if (is_flag_set(flags, decltype(flags)::enable_non_persistent))
                parts.emplace_back("enable_non_persistent");
            if (is_flag_set(flags, decltype(flags)::isolate_vswitch))
                parts.emplace_back("isolate_vswitch");
            if (is_flag_set(flags, decltype(flags)::enable_flow_steering))
                parts.emplace_back("enable_flow_steering");
            if (is_flag_set(flags, decltype(flags)::disable_sharing))
                parts.emplace_back("disable_sharing");
            if (is_flag_set(flags, decltype(flags)::enable_firewall))
                parts.emplace_back("enable_firewall");
            if (is_flag_set(flags, decltype(flags)::disable_host_port))
                parts.emplace_back("disable_host_port");
            if (is_flag_set(flags, decltype(flags)::enable_iov))
                parts.emplace_back("enable_iov");
        }

        return fmt::format_to(ctx.out(), "{}", fmt::join(parts, " | "));
    }
};
