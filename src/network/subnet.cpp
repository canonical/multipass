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
#include <multipass/platform.h>
#include <multipass/subnet.h>
#include <multipass/utils.h>

#include <fmt/format.h>

#include <algorithm>
#include <limits>

namespace mp = multipass;

namespace
{
[[nodiscard]] mp::Subnet parse(const std::string& cidr_string)
try
{
    if (auto i = cidr_string.find('/'); i != std::string::npos)
    {
        mp::IPAddress addr{cidr_string.substr(0, i)};

        const auto prefix_length = std::stoul(cidr_string.substr(i + 1));
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
    return mp::IPAddress{ip.as_uint32() & mask.as_uint32()};
}
} // namespace

mp::Subnet::Subnet(IPAddress ip, PrefixLength prefix_length) : ip_address(ip), prefix(prefix_length)
{
}

mp::Subnet::Subnet(const std::string& cidr_string) : Subnet(parse(cidr_string))
{
}

mp::IPAddress mp::Subnet::min_address() const
{
    // Subnet masks of /31 or /32 don't have separate network addresses (see RFC 3021). All others
    // do, so exclude the network address in those cases.
    return prefix < 31 ? masked_address() + 1 : masked_address();
}

mp::IPAddress mp::Subnet::max_address() const
{
    const auto mask = get_subnet_mask(prefix);
    const auto largest = ip_address.as_uint32() | ~mask.as_uint32();

    // Subnets of /31 or /32 don't have broadcast addresses (see RFC 3021). All others do, so
    // exclude the broadcast address in those cases.
    return mp::IPAddress{prefix < 31 ? largest - 1 : largest};
}

uint32_t mp::Subnet::usable_address_count() const
{
    return max_address().as_uint32() - min_address().as_uint32() + 1;
}

mp::IPAddress mp::Subnet::address() const
{
    return ip_address;
}

mp::IPAddress mp::Subnet::masked_address() const
{
    return apply_mask(ip_address, prefix);
}

mp::IPAddress mp::Subnet::broadcast_address() const
{
    // Subnets of /31 or /32 don't have broadcast addresses. See RFC 3021.
    if (prefix >= 31)
        throw mp::Subnet::NoBroadcastAddressError(prefix);
    const auto mask = get_subnet_mask(prefix);
    return mp::IPAddress{ip_address.as_uint32() | ~mask.as_uint32()};
}

mp::Subnet::PrefixLength mp::Subnet::prefix_length() const
{
    return prefix;
}

mp::IPAddress mp::Subnet::subnet_mask() const
{
    return ::get_subnet_mask(prefix);
}

mp::Subnet mp::Subnet::canonical() const
{
    return Subnet{masked_address(), prefix};
}

// uses CIDR notation
std::string mp::Subnet::to_cidr() const
{
    return fmt::format("{}/{}", ip_address.as_string(), prefix);
}

size_t mp::Subnet::size(mp::Subnet::PrefixLength prefix_length) const
{
    if (prefix_length < this->prefix_length())
        return 0;

    // a range with prefix /16 has 65536 prefix /32 networks,
    // a range with prefix /24 has 256 prefix /32 networks,
    // so a prefix /16 network can hold 65536 / 256 = 256 prefix /24 networks.
    // ex. 2^(24 - 16) = 256, [192.168.0.0/24, 192.168.255.0/24]
    return std::size_t{1} << (prefix_length - this->prefix_length());
}

bool mp::Subnet::contains(Subnet other) const
{
    // can't possibly contain a larger subnet
    if (other.prefix_length() < prefix)
        return false;

    return contains(other.masked_address());
}

bool mp::Subnet::contains(IPAddress ip) const
{
    return masked_address() <= ip && broadcast_address() >= ip;
}

std::strong_ordering mp::Subnet::operator<=>(const Subnet& other) const
{
    if (const auto ip_res = ip_address <=> other.ip_address; ip_res != 0)
        return ip_res;
    // note the prefix_length operands are purposely flipped
    return other.prefix <=> prefix;
}

mp::SubnetAllocator::SubnetAllocator(Subnet base_subnet, Subnet::PrefixLength prefix)
    : base_subnet(base_subnet), prefix(prefix)
{
    if (base_subnet.size(prefix) == 0)
        throw std::logic_error(
            fmt::format("A subnet with prefix length {} cannot be contained by {}",
                        prefix,
                        base_subnet));
}

mp::Subnet mp::SubnetAllocator::next_available()
{
    const size_t possible_subnets = base_subnet.size(prefix);
    const auto masked_address = base_subnet.masked_address();

    // Get the offset of the first address we should allocate, relative to the masked base address.
    const auto starting_address = apply_mask(base_subnet.address(), prefix);
    const auto starting_offset = starting_address.as_uint32() - masked_address.as_uint32();

    // Compute a mask for the bytes we can work with, i.e. the inverse of the base subnet's mask.
    const auto offset_mask = ~get_subnet_mask(base_subnet.prefix_length()).as_uint32();

    // The step size as a byte offset for each subnet to allocate.
    const std::uint32_t step = std::size_t{1} << (32 - prefix);

    while (block_idx < possible_subnets)
    {
        // Compute a new address, ensuring that we remain within the base subnet.
        mp::IPAddress address =
            masked_address + ((block_idx * step + starting_offset) & offset_mask);

        ++block_idx;
        mp::Subnet subnet{address, prefix};
        if (!MP_PLATFORM.subnet_used_locally(subnet))
            return subnet;
    }

    throw std::invalid_argument(fmt::format("{} is greater than the largest subnet block index {}",
                                            block_idx,
                                            possible_subnets - 1));
}
