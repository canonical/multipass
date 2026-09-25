/*
 * Copyright (C) Canonical, Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <hcs/hyperv_guid.h>

#include <hcs/hyperv_api_string_conversion.h>

#include <windows.h>
#include <objbase.h>

#include <fmt/xchar.h>

::GUID multipass::hyperv::guid_from_string(const std::wstring& guid_wstr)
{
    constexpr auto guid_length = 36;
    constexpr auto guid_length_with_braces = guid_length + 2;

    const auto input = [&guid_wstr]() {
        switch (guid_wstr.length())
        {
        case guid_length:
            // CLSIDFromString requires GUIDs to be wrapped with braces.
            return fmt::format(L"{{{}}}", guid_wstr);
        case guid_length_with_braces:
        {
            if (guid_wstr.front() != L'{' || guid_wstr.back() != L'}')
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

::GUID multipass::hyperv::guid_from_string(const std::string& guid_str)
{
    const std::wstring v = to_wstring(guid_str);
    return guid_from_string(v);
}
