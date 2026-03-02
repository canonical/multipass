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

/**
 * fwdecl for the Windows Sockets info struct
 */
struct WSAData;

namespace multipass
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
} // namespace multipass
