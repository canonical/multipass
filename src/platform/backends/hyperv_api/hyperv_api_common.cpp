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

#include "hyperv_api_common.h"

#include <fmt/xchar.h>

#include <cassert>
#include <codecvt>
#include <locale>
#include <stdexcept>

/**
 * Formatter for GUID type
 */
template <typename Char>
struct fmt::formatter<::GUID, Char>
{
    constexpr auto parse(basic_format_parse_context<Char>& ctx)
    {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const ::GUID& guid, FormatContext& ctx) const
    {
        // The format string is laid out char by char to allow it
        // to be used for initializing variables with different character
        // sizes.
        static constexpr Char guid_f[] = {'{', ':', '0', '8', 'x', '}', '-', '{', ':', '0', '4', 'x', '}', '-', '{',
                                          ':', '0', '4', 'x', '}', '-', '{', ':', '0', '2', 'x', '}', '{', ':', '0',
                                          '2', 'x', '}', '-', '{', ':', '0', '2', 'x', '}', '{', ':', '0', '2', 'x',
                                          '}', '{', ':', '0', '2', 'x', '}', '{', ':', '0', '2', 'x', '}', '{', ':',
                                          '0', '2', 'x', '}', '{', ':', '0', '2', 'x', '}', 0};
        return format_to(ctx.out(),
                         guid_f,
                         guid.Data1,
                         guid.Data2,
                         guid.Data3,
                         guid.Data4[0],
                         guid.Data4[1],
                         guid.Data4[2],
                         guid.Data4[3],
                         guid.Data4[4],
                         guid.Data4[5],
                         guid.Data4[6],
                         guid.Data4[7]);
    }
};

namespace multipass::hyperv
{

auto guid_from_wstring(const std::wstring& guid_wstr) -> ::GUID
{

    // Since we're using the windows.h in LEAN_AND_MEAN mode, COM-provided
    // GUID parsing functions such as CLSIDFromString are not available.

    auto iterator = guid_wstr.begin();
    ::GUID result = {};

    constexpr auto kGUIDLength = 36;
    constexpr auto kGUIDLengthWithBraces = kGUIDLength + 2;
    constexpr auto kGUIDFormatString = L"%8x-%4hx-%4hx-%2hhx%2hhx-%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx";
    constexpr auto kGUIDFieldCount = 11;

    if (guid_wstr.length() == kGUIDLengthWithBraces)
    {

        if (guid_wstr[0] != L'{' || guid_wstr[guid_wstr.length() - 1] != L'}')
        {
            throw std::invalid_argument{"GUID string with brances either does not start or end with a brace."};
        }

        // Skip the initial brace character
        std::advance(iterator, 1);
    }
    else if (guid_wstr.length() != kGUIDLength)
    {
        throw std::domain_error(fmt::format("Invalid GUID string {}.", guid_wstr.length()));
    }

    const auto match_count = swscanf_s(&(*iterator),
                                       kGUIDFormatString,
                                       &result.Data1,
                                       &result.Data2,
                                       &result.Data3,
                                       &result.Data4[0],
                                       &result.Data4[1],
                                       &result.Data4[2],
                                       &result.Data4[3],
                                       &result.Data4[4],
                                       &result.Data4[5],
                                       &result.Data4[6],
                                       &result.Data4[7]);

    // Ensure that the swscanf parsed the exact amount of fields
    if (!(match_count == kGUIDFieldCount))
    {
        throw std::runtime_error("Failed to parse GUID string");
    }

    return result;
}

// ---------------------------------------------------------

auto string_to_wstring(const std::string& str) -> std::wstring
{
    return std::wstring_convert<std::codecvt_utf8<wchar_t>>().from_bytes(str);
}

// ---------------------------------------------------------

auto guid_from_string(const std::string& guid_str) -> GUID
{
    // Just use the wide string overload.
    return guid_from_wstring(string_to_wstring(guid_str));
}

// ---------------------------------------------------------

auto guid_to_string(const ::GUID& guid) -> std::string
{

    return fmt::format("{}", guid);
}

// ---------------------------------------------------------

auto guid_to_wstring(const ::GUID& guid) -> std::wstring
{
    return fmt::format(L"{}", guid);
}

} // namespace multipass::hyperv
