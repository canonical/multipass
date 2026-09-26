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

#pragma once

#include <multipass/exceptions/formatted_exception_base.h>

#include <guiddef.h>

#include <string>

namespace multipass::hyperv
{
struct GuidParseError : multipass::FormattedExceptionBase<>
{
    using FormattedExceptionBase<>::FormattedExceptionBase;
};

/**
 * Parse given GUID string into a GUID struct.
 *
 * @param guid_wstr GUID in wide string form, either 36 characters
 *                  (without braces) or 38 characters (with braces.)
 *
 * @return GUID The parsed GUID
 * @throws GuidParseError if the string is not a valid GUID
 */
[[nodiscard]] ::GUID guid_from_string(const std::wstring& guid_wstr);

/**
 * Parse given GUID string into a GUID struct.
 *
 * @param guid_str GUID in string form, either 36 characters
 *                 (without braces) or 38 characters (with braces.)
 *
 * @return GUID The parsed GUID
 * @throws GuidParseError if the string is not a valid GUID
 */
[[nodiscard]] ::GUID guid_from_string(const std::string& guid_str);
} // namespace multipass::hyperv
