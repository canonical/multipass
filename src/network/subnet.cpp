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
constexpr auto large_CIDR_err_fmt = "CIDR value must be non-negative and less than 31: {}";

mp::Subnet parse(const std::string& cidr_string)
try
{
    if (auto i = cidr_string.find('/'); i != std::string::npos)
    {
        mp::IPAddress addr{cidr_string.substr(0, i)};

        auto cidr = std::stoul(cidr_string.substr(i + 1));
        if (cidr >= 31)
            throw std::invalid_argument(fmt::format(large_CIDR_err_fmt, cidr));

        return mp::Subnet(addr, cidr);
    }
    throw std::invalid_argument(
        fmt::format("CIDR address {} does not contain '/' seperator", cidr_string));
}
catch (const std::out_of_range& e)
{
    throw std::invalid_argument(fmt::format(large_CIDR_err_fmt, e.what()));
}

[[nodiscard]] mp::IPAddress get_subnet_mask(uint8_t cidr)
{
    using Octects = decltype(mp::IPAddress::octets);
    static constexpr auto value_size = sizeof(Octects::value_type) * 8;

    Octects octets{};
    std::fill(octets.begin(), octets.end(), std::numeric_limits<Octects::value_type>::max());

    if (cidr > value_size * octets.size())
        throw std::out_of_range("CIDR too large for address space");

    const uint8_t start_octet = cidr / value_size;
    const uint8_t remain = cidr % value_size;

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

[[nodiscard]] mp::IPAddress apply_mask(mp::IPAddress ip, uint8_t cidr)
{
    const auto mask = get_subnet_mask(cidr);
    for (size_t i = 0; i < ip.octets.size(); ++i)
    {
        ip.octets[i] &= mask.octets[i];
    }
    return ip;
}

mp::Subnet generate_random_subnet(uint8_t cidr, mp::Subnet range)
{
    if (cidr >= 31)
        throw std::invalid_argument(fmt::format(large_CIDR_err_fmt, cidr));

    if (cidr < range.CIDR())
        throw std::logic_error(fmt::format("A subnet with cidr {} cannot be contained by {}",
                                           cidr,
                                           range.as_string()));

    // ex. 2^(24 - 16) = 256, [192.168.0.0/24, 192.168.255.0/24]
    const size_t possibleSubnets = std::size_t{1} << (cidr - range.CIDR());

    // narrowing conversion, possibleSubnets is guaranteed to be < 2^31 (4 bytes is safe)
    static_assert(sizeof(decltype(MP_UTILS.random_int(0, possibleSubnets))) >= 4);

    const auto subnet = static_cast<size_t>(MP_UTILS.random_int(0, possibleSubnets - 1));

    // ex. 192.168.0.0 + (4 * 2^(32 - 24)) = 192.168.0.0 + 1024 = 192.168.4.0
    mp::IPAddress id = range.identifier() + (subnet * (std::size_t{1} << (32 - cidr)));

    return mp::Subnet{id, cidr};
}
} // namespace

mp::Subnet::Subnet(IPAddress ip, uint8_t cidr) : id(apply_mask(ip, cidr)), cidr(cidr)
{
    if (cidr >= 31)
    {
        throw std::invalid_argument(
            fmt::format("CIDR value must be non-negative and less than 31: {}", cidr));
    }
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
    // identifier + 2^(32 - cidr) - 1 - 1
    return id + ((1ull << (32ull - cidr)) - 2ull);
}

uint32_t mp::Subnet::address_count() const
{
    return max_address().as_uint32() - min_address().as_uint32() + 1;
}

mp::IPAddress mp::Subnet::identifier() const
{
    return id;
}

uint8_t mp::Subnet::CIDR() const
{
    return cidr;
}

mp::IPAddress mp::Subnet::subnet_mask() const
{
    return ::get_subnet_mask(cidr);
}

// uses CIDR notation
std::string mp::Subnet::as_string() const
{
    return fmt::format("{}/{}", id.as_string(), cidr);
}

bool mp::Subnet::contains(Subnet other) const
{
    // can't possibly contain a larger subnet
    if (other.CIDR() < CIDR())
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
    return id == other.id && cidr == other.cidr;
}

bool mp::Subnet::operator<(const Subnet& other) const
{
    // note cidr comparison is flipped, smaller is bigger
    return id < other.id || (id == other.id && cidr > other.cidr);
}

mp::Subnet mp::SubnetUtils::generate_random_subnet(uint8_t cidr, Subnet range) const
{
    // @TODO don't rely on pure randomness
    for (auto i = 0; i < 100; ++i)
    {
        const auto subnet = ::generate_random_subnet(cidr, range);
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
