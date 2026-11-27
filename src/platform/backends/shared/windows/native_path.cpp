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

#include <shared/windows/native_path.h>
#include <shared/windows/wchar_conversion.h>

#include <boost/json.hpp>

#include <string_view>
#include <type_traits>

using multipass::NativePath;

template <typename Char>
template <typename FormatContext>
auto fmt::formatter<NativePath, Char>::format(const NativePath& path, FormatContext& ctx) const
    -> FormatContext::iterator
{
    const std::u8string u8_path = path.get().u8string();
    // Boost.JSON assumes UTF-8 but does not treat char8_t as a character type.
    const std::string_view u8_path_view = {reinterpret_cast<const char*>(u8_path.data()),
                                           u8_path.size()};
    const std::string json_path = boost::json::serialize(u8_path_view);

    // Serialization adds quotes as well as escaping the string.
    std::string_view escaped_path{json_path};
    escaped_path.remove_prefix(1);
    escaped_path.remove_suffix(1);

    if constexpr (std::is_same_v<Char, char>)
    {
        return formatter<basic_string_view<Char>, Char>::format(
            basic_string_view<Char>{escaped_path.data(), escaped_path.size()},
            ctx);
    }
    else if constexpr (std::is_same_v<Char, wchar_t>)
    {
        const auto wide_path = multipass::utf8_to_wchar(escaped_path);

        return formatter<basic_string_view<Char>, Char>::format(
            basic_string_view<Char>{wide_path.data(), wide_path.size()},
            ctx);
    }
    else
    {
        static_assert(std::is_same_v<Char, char> || std::is_same_v<Char, wchar_t>,
                      "Unsupported character type");
    }
}

template auto fmt::formatter<NativePath, char>::format<fmt::format_context>(
    const NativePath&,
    fmt::format_context&) const -> fmt::format_context::iterator;

template auto fmt::formatter<NativePath, wchar_t>::format<fmt::wformat_context>(
    const NativePath&,
    fmt::wformat_context&) const -> fmt::wformat_context::iterator;
