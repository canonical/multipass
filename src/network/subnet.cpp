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
    const uint32_t mask = (prefix_length == 0) ? 0 : ~uint32_t{0} << (32 - prefix_length);
    return mp::IPAddress{mask};
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

mp::Subnet random_subnet_from_range(mp::Subnet::PrefixLength prefix_length, mp::Subnet range)
{
    if (prefix_length < range.prefix_length())
        throw std::logic_error(
            fmt::format("A subnet with prefix length {} cannot be contained by {}",
                        prefix_length,
                        range));

    // a range with prefix /16 has 65536 prefix /32 networks,
    // a range with prefix /24 has 256 prefix /32 networks,
    // so a prefix /16 network can hold 65536 / 256 = 256 prefix /24 networks.
    // ex. 2^(24 - 16) = 256, [192.168.0.0/24, 192.168.255.0/24]
    const size_t possible_subnets = std::size_t{1} << (prefix_length - range.prefix_length());

    // narrowing conversion, possibleSubnets is guaranteed to be < 2^31 (4 bytes is safe)
    static_assert(sizeof(decltype(MP_UTILS.random_int(0, possible_subnets))) >= 4);

    const auto subnet_block_idx = static_cast<size_t>(MP_UTILS.random_int(0, possible_subnets - 1));

    // ex. 192.168.0.0 + (4 * 2^(32 - 24)) = 192.168.0.0 + 1024 = 192.168.4.0
    mp::IPAddress id =
        range.network_address() + (subnet_block_idx * (std::size_t{1} << (32 - prefix_length)));

    return mp::Subnet{id, prefix_length};
}
} // namespace

mp::Subnet::Subnet(IPAddress ip, PrefixLength prefix_length)
    : address(apply_mask(ip, prefix_length)), prefix(prefix_length)
{
}

mp::Subnet::Subnet(const std::string& cidr_string) : Subnet(parse(cidr_string))
{
}

mp::IPAddress mp::Subnet::min_address() const
{
    return address + 1;
}

mp::IPAddress mp::Subnet::max_address() const
{
    // address + 2^(32 - prefix) - 1 - 1
    // address + 2^(32 - prefix) is the next subnet's network address for this prefix length
    // subtracting 1 to stay in this subnet and another 1 to exclude the broadcast address
    return address + ((1ull << (32ull - prefix)) - 2ull);
}

uint32_t mp::Subnet::usable_address_count() const
{
    return max_address().as_uint32() - min_address().as_uint32() + 1;
}

mp::IPAddress mp::Subnet::network_address() const
{
    return address;
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
    return fmt::format("{}/{}", address.as_string(), prefix);
}

bool mp::Subnet::contains(Subnet other) const
{
    // can't possibly contain a larger subnet
    if (other.prefix_length() < prefix)
        return false;

    return contains(other.network_address());
}

bool mp::Subnet::contains(IPAddress ip) const
{
    // since get_max_address doesn't include the broadcast address add 1 to it.
    return address <= ip && (max_address() + 1) >= ip;
}

std::strong_ordering mp::Subnet::operator<=>(const Subnet& other) const
{
    const auto ip_res = address <=> other.address;

    // note the prefix_length operands are purposely flipped
    return (ip_res == 0) ? other.prefix_length() <=> prefix_length() : ip_res;
}

mp::Subnet mp::SubnetUtils::random_subnet_from_range(Subnet::PrefixLength prefix,
                                                     Subnet range) const
{
    // @TODO don't rely on pure randomness
    for (auto i = 0; i < 100; ++i)
    {
        const auto subnet = ::random_subnet_from_range(prefix, range);
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
