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

#include "multipass/platform.h"

#include <multipass/file_ops.h>
#include <multipass/subnet.h>
#include <multipass/utils.h>

#include <fmt/format.h>

#include <algorithm>
#include <limits>

namespace mp = multipass;

namespace
{
mp::Subnet parse(const std::string& cidr_string)
try
{
    if (auto i = cidr_string.find('/'); i != std::string::npos)
    {
        mp::IPAddress addr{cidr_string.substr(0, i)};

        const auto prefix_length = std::stoul(cidr_string.substr(i + 1));
        if (prefix_length >= 31)
            throw mp::Subnet::PrefixLengthOutOfRange(prefix_length);

        return mp::Subnet(addr, prefix_length);
    }
    throw std::invalid_argument(
        fmt::format("CIDR {:?} does not contain '/' seperator", cidr_string));
}
catch (const mp::Subnet::PrefixLengthOutOfRange&)
{
    throw;
}
catch (const std::out_of_range& e)
{
    throw mp::Subnet::PrefixLengthOutOfRange(e.what());
}

[[nodiscard]] mp::IPAddress get_subnet_mask(mp::Subnet::PrefixLength prefix_length)
{
    using Octects = decltype(mp::IPAddress::octets);
    static constexpr auto value_size = sizeof(Octects::value_type) * 8;

    Octects octets{};
    std::fill(octets.begin(), octets.end(), std::numeric_limits<Octects::value_type>::max());

    if (prefix_length > value_size * octets.size())
        throw std::out_of_range("prefix length too large for address space");

    const uint8_t start_octet = prefix_length / value_size;
    const uint8_t remain = prefix_length % value_size;

    for (size_t i = start_octet; i < octets.size(); ++i)
    {
        octets[i] = 0;
    }

    if (remain != 0)
    {
        assert(start_octet < octets.size()); // sanity

        // remain = 5, 1 << (8 - 5) = 00001000 -> 00000111 -> 11111000
        octets[start_octet] = ~((1u << (value_size - remain)) - 1u);
    }

    return mp::IPAddress{octets};
}

[[nodiscard]] mp::IPAddress apply_mask(mp::IPAddress ip, mp::Subnet::PrefixLength prefix_length)
{
    const auto mask = get_subnet_mask(prefix_length);
    for (size_t i = 0; i < ip.octets.size(); ++i)
    {
        ip.octets[i] &= mask.octets[i];
    }
    return ip;
}

mp::Subnet generate_random_subnet(mp::Subnet::PrefixLength prefix_length, mp::Subnet range)
{
    if (prefix_length < range.prefix_length())
        throw std::logic_error(
            fmt::format("A subnet with prefix length {} cannot be contained by {}",
                        prefix_length,
                        range));

    // ex. 2^(24 - 16) = 256, [192.168.0.0/24, 192.168.255.0/24]
    const size_t possibleSubnets = std::size_t{1} << (prefix_length - range.prefix_length());

    // narrowing conversion, possibleSubnets is guaranteed to be < 2^31 (4 bytes is safe)
    static_assert(sizeof(decltype(MP_UTILS.random_int(0, possibleSubnets))) >= 4);

    const auto subnet = static_cast<size_t>(MP_UTILS.random_int(0, possibleSubnets - 1));

    // ex. 192.168.0.0 + (4 * 2^(32 - 24)) = 192.168.0.0 + 1024 = 192.168.4.0
    mp::IPAddress id = range.identifier() + (subnet * (std::size_t{1} << (32 - prefix_length)));

    return mp::Subnet{id, prefix_length};
}
} // namespace

mp::Subnet::Subnet(IPAddress ip, PrefixLength prefix_length)
    : id(apply_mask(ip, prefix_length)), prefix(prefix_length)
{
}

mp::Subnet::Subnet(const std::string& cidr_string) : Subnet(parse(cidr_string))
{
}

mp::IPAddress mp::Subnet::min_address() const
{
    return id + 1;
}

mp::IPAddress mp::Subnet::max_address() const
{
    // identifier + 2^(32 - prefix) - 1 - 1
    return id + ((1ull << (32ull - prefix)) - 2ull);
}

uint32_t mp::Subnet::address_count() const
{
    return max_address().as_uint32() - min_address().as_uint32() + 1;
}

mp::IPAddress mp::Subnet::identifier() const
{
    return id;
}

mp::Subnet::PrefixLength mp::Subnet::prefix_length() const
{
    return prefix;
}

mp::IPAddress mp::Subnet::subnet_mask() const
{
    return ::get_subnet_mask(prefix);
}

// uses CIDR notation
std::string mp::Subnet::to_cidr() const
{
    return fmt::format("{}/{}", id.as_string(), prefix);
}

bool mp::Subnet::contains(Subnet other) const
{
    // can't possibly contain a larger subnet
    if (other.prefix_length() < prefix)
        return false;

    return contains(other.identifier());
}

bool mp::Subnet::contains(IPAddress ip) const
{
    // since get_max_address doesn't include the broadcast address add 1 to it.
    return identifier() <= ip && (max_address() + 1) >= ip;
}

bool mp::Subnet::operator==(const Subnet& other) const
{
    return id == other.id && prefix == other.prefix;
}

/* TODO C++20 uncomment
std::strong_ordering mp::Subnet::operator<=>(const Subnet& other) const
{
    const auto ip_res = id <=> other.id;

    // note the prefix_length operands are purposely flipped
    return (ip_res == 0) ? other.prefix_length <=> prefix_length : ip_res;
}
*/

mp::Subnet mp::SubnetUtils::generate_random_subnet(Subnet::PrefixLength prefix, Subnet range) const
{
    // @TODO don't rely on pure randomness
    for (auto i = 0; i < 100; ++i)
    {
        const auto subnet = ::generate_random_subnet(prefix, range);
        if (MP_PLATFORM.subnet_used_locally(subnet))
            continue;

        if (MP_PLATFORM.can_reach_gateway(subnet.min_address()))
            continue;

        if (MP_PLATFORM.can_reach_gateway(subnet.max_address()))
            continue;

        return subnet;
    }

    throw std::runtime_error("Could not determine a subnet for networking.");
}
