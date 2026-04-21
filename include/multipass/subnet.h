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

#include <multipass/exceptions/formatted_exception_base.h>
#include <multipass/path.h>
#include <multipass/singleton.h>

#include <boost/json.hpp>

#include "ip_address.h"

namespace multipass
{
class Subnet
{
public:
    struct PrefixLengthOutOfRange final : FormattedExceptionBase<std::out_of_range>
    {
        template <class T>
        explicit PrefixLengthOutOfRange(const T& value)
            // Subnet masks of /31 or /32 require some special handling that we don't support.
            : FormattedExceptionBase{
                  "Subnet prefix length must be non-negative and less than 31: {}",
                  value}
        {
        }
    };

    class PrefixLength
    {
    public:
        constexpr PrefixLength(uint8_t value) : value(value)
        {
            if (value >= 31)
                throw PrefixLengthOutOfRange{value};
        }

        constexpr operator uint8_t() const noexcept
        {
            return value;
        }

    private:
        uint8_t value;
    };

    Subnet(IPAddress ip, PrefixLength prefix_length);

    Subnet(const std::string& cidr_string);

    // Return the smallest IP address available in this subnet
    [[nodiscard]] IPAddress min_address() const;
    // Return the largest IP address available in this subnet, excluding the broadcast address
    [[nodiscard]] IPAddress max_address() const;
    // Return the number of usable IP addresses in this subnet
    [[nodiscard]] uint32_t usable_address_count() const;

    // Return the original IP address
    [[nodiscard]] IPAddress address() const;
    // Return the IP address with the subnet mask applied
    [[nodiscard]] IPAddress masked_address() const;
    // Return the broadcast address for this subnet
    [[nodiscard]] IPAddress broadcast_address() const;
    // Return the prefix length, e.g. the 24 in 192.168.1.0/24
    [[nodiscard]] PrefixLength prefix_length() const;
    // Return the subnet mask as an IP, e.g. 255.255.255.0
    [[nodiscard]] IPAddress subnet_mask() const;

    // Return this subnet with the subnet mask applied to the IP address
    [[nodiscard]] Subnet canonical() const;

    // Return a string representing this subnet in CIDR notation
    [[nodiscard]] std::string to_cidr() const;

    // Return the number of subnets of size `prefix_length` that fit in this subnet
    [[nodiscard]] size_t size(PrefixLength prefix_length) const;

    // Subnets are either disjoint or the smaller is a subset of the larger
    [[nodiscard]] bool contains(Subnet other) const;
    [[nodiscard]] bool contains(IPAddress ip) const;

    [[nodiscard]] std::strong_ordering operator<=>(const Subnet& other) const;
    [[nodiscard]] bool operator==(const Subnet& other) const = default;

    friend void tag_invoke(const boost::json::value_from_tag&,
                           boost::json::value& json,
                           const Subnet& subnet)
    {
        json = subnet.to_cidr();
    }

    friend Subnet tag_invoke(const boost::json::value_to_tag<Subnet>&,
                             const boost::json::value& json)
    {
        return value_to<std::string>(json);
    }

private:
    IPAddress ip_address;
    PrefixLength prefix;
};

// Allocate child subnets from a base subnet
class SubnetAllocator
{
public:
    SubnetAllocator(Subnet base_subnet, Subnet::PrefixLength prefix);

    [[nodiscard]] Subnet next_available();

private:
    Subnet base_subnet;
    Subnet::PrefixLength prefix;
    size_t block_idx = 0;
};
} // namespace multipass

namespace fmt
{
template <>
struct formatter<multipass::Subnet> : formatter<std::string_view>
{
    template <typename FormatContext>
    auto format(const multipass::Subnet& subnet, FormatContext& ctx) const
    {
        return format_to(ctx.out(), "{}", subnet.to_cidr());
    }
};

template <>
struct formatter<multipass::Subnet::PrefixLength> : formatter<std::string_view>
{
    template <typename FormatContext>
    auto format(const multipass::Subnet::PrefixLength& prefix, FormatContext& ctx) const
    {
        return format_to(ctx.out(), "{}", uint8_t(prefix));
    }
};
} // namespace fmt
