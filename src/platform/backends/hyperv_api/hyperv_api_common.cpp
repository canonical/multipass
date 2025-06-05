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

#include <hyperv_api/hyperv_api_common.h>
#include <multipass/exceptions/formatted_exception_base.h>

#include <fmt/xchar.h>

#include <objbase.h> // for CLSIDFromString

#include <codecvt>
#include <locale>

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

struct GuidParseError : FormattedExceptionBase<>
{
    using FormattedExceptionBase<>::FormattedExceptionBase;
};

auto guid_from_wstring(const std::wstring& guid_wstr) -> ::GUID
{
    constexpr auto kGUIDLength = 36;
    constexpr auto kGUIDLengthWithBraces = kGUIDLength + 2;

    const auto input = [&guid_wstr]() {
        switch (guid_wstr.length())
        {
        case kGUIDLength:
            // CLSIDFromString requires GUIDs to be wrapped with braces.
            return fmt::format(L"{{{}}}", guid_wstr);
        case kGUIDLengthWithBraces:
        {
            if (*guid_wstr.begin() != L'{' || *std::prev(guid_wstr.end()) != L'}')
            {
                throw GuidParseError{"GUID string either does not start or end with a brace."};
            }
            return guid_wstr;
        }
        }
        throw GuidParseError{"Invalid length for a GUID string ({}).", guid_wstr.length()};
    }();

    ::GUID guid = {};

    const auto result = CLSIDFromString(input.c_str(), &guid);

    if (FAILED(result))
    {
        throw GuidParseError{"Failed to parse the GUID string ({}).", result};
    }

    return guid;
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
