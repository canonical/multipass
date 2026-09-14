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

#include <hyperv_api/hcs/hyperv_hcs_path.h>
#include <hyperv_api/hyperv_api_string_conversion.h>

#include <boost/json.hpp>

#include <string_view>

using multipass::hyperv::hcs::HcsPath;

template <typename Char>
template <typename FormatContext>
auto fmt::formatter<HcsPath, Char>::format(const HcsPath& path, FormatContext& ctx) const
    -> FormatContext::iterator
{
    const std::u8string u8_path = path.get().u8string();
    // boost::json::value assumes UTF-8 encoding, but it doesn't work with
    // std::u8string as it assumes that the underlying type is char and not char8_t
    const std::string_view u8_path_view = {reinterpret_cast<const char*>(u8_path.data()),
                                           u8_path.size()};

    const std::string json_path = boost::json::serialize(u8_path_view);

    // json_path is quoted as well as escaped, so we remove the opening and closing quotes.
    std::string_view escaped_path{json_path};
    escaped_path.remove_prefix(1);
    escaped_path.remove_suffix(1);

    if constexpr (std::is_same_v<Char, char>)
        return fmt::format_to(ctx.out(), "{}", escaped_path);
    else
        return fmt::format_to(ctx.out(), L"{}", multipass::hyperv::to_wstring(escaped_path));
}

template auto fmt::formatter<HcsPath, char>::format<fmt::format_context>(const HcsPath&,
                                                                         fmt::format_context&) const
    -> fmt::format_context::iterator;

template auto fmt::formatter<HcsPath, wchar_t>::format<fmt::wformat_context>(
    const HcsPath&,
    fmt::wformat_context&) const -> fmt::wformat_context::iterator;
