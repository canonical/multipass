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

#include <multipass/subnet.h>

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
    throw std::invalid_argument(fmt::format("CIDR address {} does not contain '/' seperator", cidr_string));
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

    for (size_t i = 0; i < remain; ++ i)
    {
        octets[start_octet] >>= 1;
        octets[start_octet] |= 1 << (value_size - 1);
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

// due to how subnets work overlap does not need consideration
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

mp::Subnet mp::SubnetUtils::generate_random_subnet(IPAddress start,
                                                      IPAddress end,
                                                      uint8_t cidr) const
{
    return mp::Subnet{""};
}

mp::Subnet mp::SubnetUtils::get_subnet(const mp::Path& network_dir, const QString& bridge_name) const
{
    return mp::Subnet{""};
}
