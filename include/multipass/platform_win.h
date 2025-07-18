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

#pragma once

#include <windows.h>

#include <fmt/format.h>

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

} // namespace multipass::platform

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
        static constexpr Char guid_f[] = {
            '{', ':', '0', '8', 'x', '}', '-', '{', ':', '0', '4', 'x', '}', '-', '{',
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
