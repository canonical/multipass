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

#include <multipass/ip_address.h>

#include <ranges>
#include <sstream>
#include <stdexcept>

#include <fmt/format.h>

namespace mp = multipass;

namespace
{
uint8_t as_octet(uint32_t value)
{
    return static_cast<uint8_t>(value);
}

bool is_valid_octet(int value)
{
    return value >= 0 && value < 256;
}

std::array<uint8_t, 4> parse(const std::string& ip)
{
    // FIXME: Use Boost.ASIO?
    std::array sep = {'\0', '\0', '\0'};
    std::array octets = {(int)-1, -1, -1, -1};

    std::stringstream s(ip);
    s >> octets[0] >> sep[0] >> octets[1] >> sep[1] >> octets[2] >> sep[2] >> octets[3];

    if (!std::ranges::all_of(octets, is_valid_octet) ||
        !std::ranges::all_of(sep, [](char c) { return c == '.'; }))
        throw std::invalid_argument(fmt::format("invalid IP address {}", ip));

    return {{as_octet(octets[0]), as_octet(octets[1]), as_octet(octets[2]), as_octet(octets[3])}};
}

std::array<uint8_t, 4> to_octets(uint32_t value)
{
    return {
        {as_octet(value >> 24u), as_octet(value >> 16u), as_octet(value >> 8u), as_octet(value)}};
}
} // namespace

mp::IPAddress::IPAddress(std::array<uint8_t, 4> octets) : octets{octets}
{
}

mp::IPAddress::IPAddress(const std::string& ip_string) : IPAddress(parse(ip_string))
{
}

mp::IPAddress::IPAddress(uint32_t value) : IPAddress(to_octets(value))
{
}

std::string mp::IPAddress::as_string() const
{
    std::stringstream ip;
    ip << static_cast<int>(octets[0]) << "." << static_cast<int>(octets[1]) << "."
       << static_cast<int>(octets[2]) << "." << static_cast<int>(octets[3]);
    return ip.str();
}

uint32_t mp::IPAddress::as_uint32() const
{
    uint32_t value = octets[0] << 24u;
    value |= octets[1] << 16u;
    value |= octets[2] << 8u;
    value |= octets[3];
    return value;
}

// uint8_t is not required to support <=> by the standard. Appease Apple clang.
std::strong_ordering mp::IPAddress::operator<=>(const IPAddress& other) const
{
    return as_uint32() <=> other.as_uint32();
}

mp::IPAddress mp::IPAddress::operator+(int value) const
{
    return mp::IPAddress(as_uint32() + value);
}
