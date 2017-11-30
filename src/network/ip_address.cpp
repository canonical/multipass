/*
 * Copyright (C) 2017 Canonical, Ltd.
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

#include <multipass/ip_address.h>

#include <sstream>

namespace mp = multipass;

namespace
{
uint8_t as_octect(int value)
{
    if (value < 0 || value > 255)
        throw std::invalid_argument("invalid IP octet");

    return static_cast<uint8_t>(value);
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

    // Double brackets here because clang disklikes it even though it's perfectly valid
    return {{as_octect(a), as_octect(b), as_octect(c), as_octect(d)}};
}

auto to_octets(uint32_t value)
{
    std::array<uint8_t, 4> octets;
    octets[0] = as_octect((value >> 24) & 0xFF);
    octets[1] = as_octect((value >> 16) & 0xFF);
    octets[2] = as_octect((value >> 8) & 0xFF);
    octets[3] = as_octect((value >> 0) & 0xFF);
    return octets;
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
    uint32_t value = octets[0] << 24 | octets[1] << 16 | octets[2] << 8 | octets[3];
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
