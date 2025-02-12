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

#ifndef MULTIPASS_HYPERV_API_COMMON_H
#define MULTIPASS_HYPERV_API_COMMON_H

#include <string>
#include <windows.h>

namespace multipass::hyperv
{

// ---------------------------------------------------------

/**
 * Parse given GUID string into a GUID struct.
 *
 * @param guid_str GUID in string form, either 36 characters
 *                  (without braces) or 38 characters (with braces.)
 *
 * @return GUID The parsed GUID
 */
[[nodiscard]] auto guid_from_string(const std::string& guid_str) -> GUID;

/**
 * Parse given GUID string into a GUID struct.
 *
 * @param guid_wstr GUID in string form, either 36 characters
 *                  (without braces) or 38 characters (with braces.)
 *
 * @return GUID The parsed GUID
 */
[[nodiscard]] auto guid_from_wstring(const std::wstring& guid_wstr) -> GUID;

// ---------------------------------------------------------

/**
 * @brief Convert a GUID to its string representation
 *
 * @param [in] guid GUID to convert
 * @return std::string GUID in string form
 */
[[nodiscard]] auto guid_to_string(const ::GUID& guid) -> std::string;

// ---------------------------------------------------------

/**
 * @brief Convert a guid to its wide string representation
 *
 * @param [in] guid GUID to convert
 * @return std::wstring GUID in wstring form
 */
[[nodiscard]] auto guid_to_wstring(const ::GUID& guid) -> std::wstring;

// ---------------------------------------------------------

/**
 * Convert a multi-byte string to a wide-character string.
 *
 * @param str Multi-byte string
 * @return Wide-character equivalent of the given multi-byte string.
 */
[[nodiscard]] auto string_to_wstring(const std::string& str) -> std::wstring;

} // namespace multipass::hyperv

#endif