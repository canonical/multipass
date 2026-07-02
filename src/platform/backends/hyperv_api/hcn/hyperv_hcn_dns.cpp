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

#include <hyperv_api/hcn/hyperv_hcn_dns.h>

#include <hyperv_api/hyperv_api_string_conversion.h>

#include <type_traits>

using namespace multipass::hyperv;
using namespace multipass::hyperv::hcn;

namespace
{
/**
 * Render a list of strings as the contents of a JSON string array,
 * e.g. {"a", "b"} -> `"a","b"`.
 */
template <typename Char>
std::basic_string<Char> as_json_string_array(const std::vector<std::string>& values)
{
    std::basic_string<Char> result{};
    bool first = true;
    for (const auto& value : values)
    {
        if (!first)
            result += static_cast<Char>(',');
        first = false;
        result += static_cast<Char>('"');
        if constexpr (std::is_same_v<Char, wchar_t>)
            result += to_wstring(std::string_view{value});
        else
            result += value;
        result += static_cast<Char>('"');
    }
    return result;
}
} // namespace

template <typename Char>
template <typename FormatContext>
auto fmt::formatter<HcnDns, Char>::format(const HcnDns& dns, FormatContext& ctx) const
    -> FormatContext::iterator
{
    static constexpr auto json_template = string_literal<Char>(R"json(
        {{
            "Domain": "{0}",
            "Search": [{1}],
            "ServerList": [{2}],
            "Options": [{3}]
        }})json");

    return json_template.format_to(ctx,
                                   dns.domain,
                                   as_json_string_array<Char>(dns.search),
                                   as_json_string_array<Char>(dns.server_list),
                                   as_json_string_array<Char>(dns.options));
}

template auto fmt::formatter<HcnDns, char>::format<fmt::format_context>(
    const HcnDns&,
    fmt::format_context&) const -> fmt::format_context::iterator;

template auto fmt::formatter<HcnDns, wchar_t>::format<fmt::wformat_context>(
    const HcnDns&,
    fmt::wformat_context&) const -> fmt::wformat_context::iterator;
