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
 * Authored by: Alberto Aguirre <alberto.aguirre@canonical.com>
 *
 */

#pragma once

#include <array>
#include <cstdint>
#include <string>

namespace multipass
{
struct IPAddress
{
    explicit IPAddress(std::array<uint8_t, 4> octets);
    explicit IPAddress(const std::string& ip_string);
    explicit IPAddress(uint32_t value);

    [[nodiscard]] std::string as_string() const;
    [[nodiscard]] uint32_t as_uint32() const;

    bool operator==(const IPAddress& other) const;
    bool operator!=(const IPAddress& other) const;
    bool operator<(const IPAddress& other) const;
    bool operator<=(const IPAddress& other) const;
    bool operator>(const IPAddress& other) const;
    bool operator>=(const IPAddress& other) const;
    IPAddress operator+(int value) const;

    std::array<uint8_t, 4> octets;
};
} // namespace multipass
