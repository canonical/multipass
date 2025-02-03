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
#include <locale.h>
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
    // We could've gotten away with `return std::wstring{str.begin(), str.end()}`
    // and it'd work for 99 pct of the scenarios we'd see, but... let's do the correct thing.

    // Avoid recreating the locale.
    static std::unique_ptr<std::remove_pointer_t<_locale_t>, decltype(&_free_locale)> locale{
        _create_locale(LC_ALL, "C"),
        _free_locale};

    // Call the function with nullptr to learn how much space we need to
    // store the result.
    std::size_t required_size{0};
    if (!(0 == _mbstowcs_s_l(&required_size, nullptr, 0, str.c_str(), str.length(), locale.get())))
    {
        throw std::invalid_argument{"Failed to convert multi-byte string to wide-character string."};
    }

    // String to store the converted output.
    std::wstring result{};

    // The required_size will account for the NUL terminator.
    // Hence, the actual amount of storage needed for resize is 1
    // characters less.
    result.resize(required_size - 1, L'\0');

    // Perform the conversion.
    std::size_t converted_char_cnt{0};
    if (!(0 == _mbstowcs_s_l(&converted_char_cnt,
                             // data() returns a non-const pointer since C++17, and it is guaranteed
                             // to be contiguous memory.
                             result.data(),
                             // We're doing a size + 1 here because _mbstowcs_s_l will put a NUL terminator
                             // at the end. The string's internal buffer already account for it, and overwriting
                             // it with another NUL terminator is well-defined:

                             // Quoting cppreference (https://en.cppreference.com/w/cpp/string/basic_string/data)
                             //  ...
                             //  2) Modifying the past-the-end null terminator stored at data() + size() to any value
                             //  other than CharT() has undefined behavior.
                             //
                             //  CharT() == '\0'.
                             result.size() + 1,
                             // The multi-byte string.
                             str.c_str(),
                             // Convert at most the count of multi-byte string characters.
                             str.length(),
                             locale.get())))
    {
        throw std::invalid_argument{"Failed to convert multi-byte string to wide-character string."};
    }
    assert(converted_char_cnt == str.length() + 1);
    assert(converted_char_cnt == required_size);
    return result;
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