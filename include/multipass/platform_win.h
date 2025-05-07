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

#ifndef MULTIPASS_PLATFORM_WIN_H
#define MULTIPASS_PLATFORM_WIN_H

#include <windows.h>

#include <string>

struct WSAData;

namespace multipass::platform
{
struct wsa_init_wrapper
{
    wsa_init_wrapper();
    ~wsa_init_wrapper();

    /**
     * Check whether WSA initialization has succeeded.
     *
     * @return true WSA is initialized successfully
     * @return false WSA initialization failed
     */
    operator bool() const noexcept
    {
        return wsa_init_result == 0;
    }

private:
    WSAData* wsa_data{nullptr};
    const int wsa_init_result{-1};
};

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

} // namespace multipass::platform

#endif // MULTIPASS_PLATFORM_WIN_H
