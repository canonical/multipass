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

#include <sstream>

namespace mp = multipass;

namespace
{
uint8_t as_octet(uint32_t value)
{
    return static_cast<uint8_t>(value);
}

void check_range(int value)
{
    if (value < 0 || value > 255)
        throw std::invalid_argument("invalid IP octet");
}

std::array<uint8_t, 4> parse(const std::string& ip)
{
    char ch;
    int a = -1;
    int b = -1;
    int c = -1;
    int d = -1;
    std::stringstream s(ip);
    s >> a >> ch >> b >> ch >> c >> ch >> d;

    check_range(a);
    check_range(b);
    check_range(c);
    check_range(d);

    return {{as_octet(a), as_octet(b), as_octet(c), as_octet(d)}};
}

std::array<uint8_t, 4> to_octets(uint32_t value)
{
    return {{as_octet(value >> 24u), as_octet(value >> 16u), as_octet(value >> 8u), as_octet(value)}};
}
}

mp::IPAddress::IPAddress(std::array<uint8_t, 4> octets) : octets{octets}
{
}

mp::IPAddress::IPAddress(const std::string& ip) : IPAddress(parse(ip))
{
}

mp::IPAddress::IPAddress(uint32_t value) : IPAddress(to_octets(value))
{
}

std::string mp::IPAddress::as_string() const
{
    std::stringstream ip;
    ip << static_cast<int>(octets[0]) << "." << static_cast<int>(octets[1]) << "." << static_cast<int>(octets[2]) << "."
       << static_cast<int>(octets[3]);
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

bool mp::IPAddress::operator==(const IPAddress& other) const
{
    return octets == other.octets;
}

bool mp::IPAddress::operator!=(const IPAddress& other) const
{
    return octets != other.octets;
}

bool mp::IPAddress::operator<(const IPAddress& other) const
{
    return as_uint32() < other.as_uint32();
}

bool mp::IPAddress::operator<=(const IPAddress& other) const
{
    return as_uint32() <= other.as_uint32();
}

bool mp::IPAddress::operator>(const IPAddress& other) const
{
    return as_uint32() > other.as_uint32();
}

bool mp::IPAddress::operator>=(const IPAddress& other) const
{
    return as_uint32() >= other.as_uint32();
}

mp::IPAddress mp::IPAddress::operator+(int value) const
{
    return mp::IPAddress(as_uint32() + value);
}
