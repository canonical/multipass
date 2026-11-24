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

#include "wchar_conversion.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace mp = multipass;

std::string mp::wchar_to_utf8(std::wstring_view input)
{
    if (input.empty())
        return {};

    const auto size_needed = ::WideCharToMultiByte(CP_UTF8,
                                                   0,
                                                   input.data(),
                                                   static_cast<int>(input.size()),
                                                   nullptr,
                                                   0,
                                                   nullptr,
                                                   nullptr);
    std::string result(static_cast<std::size_t>(size_needed), '\0');
    ::WideCharToMultiByte(CP_UTF8,
                          0,
                          input.data(),
                          static_cast<int>(input.size()),
                          result.data(),
                          size_needed,
                          nullptr,
                          nullptr);
    return result;
}

std::wstring mp::utf8_to_wchar(std::string_view input)
{
    if (input.empty())
        return {};

    const auto size_needed =
        ::MultiByteToWideChar(CP_UTF8, 0, input.data(), static_cast<int>(input.size()), nullptr, 0);
    std::wstring result(static_cast<std::size_t>(size_needed), L'\0');
    ::MultiByteToWideChar(CP_UTF8,
                          0,
                          input.data(),
                          static_cast<int>(input.size()),
                          result.data(),
                          size_needed);
    return result;
}
