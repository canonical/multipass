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

#include "ip_address.h"

#define MP_SUBNET_UTILS multipass::SubnetUtils::instance()

namespace multipass
{
class Subnet
{
public:
    struct PrefixLengthOutOfRange final : FormattedExceptionBase<std::out_of_range>
    {
        template <class T>
        explicit PrefixLengthOutOfRange(const T& value)
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

    [[nodiscard]] IPAddress min_address() const;
    [[nodiscard]] IPAddress max_address() const;
    [[nodiscard]] uint32_t usable_address_count() const;

    [[nodiscard]] IPAddress network_address() const;
    [[nodiscard]] PrefixLength prefix_length() const;
    [[nodiscard]] IPAddress subnet_mask() const;

    [[nodiscard]] std::string to_cidr() const;

    // Subnets are either disjoint or the smaller is a subset of the larger
    [[nodiscard]] bool contains(Subnet other) const;
    [[nodiscard]] bool contains(IPAddress ip) const;

    [[nodiscard]] std::strong_ordering operator<=>(const Subnet& other) const;
    [[nodiscard]] bool operator==(const Subnet& other) const = default;

private:
    IPAddress address;
    PrefixLength prefix;
};

struct SubnetUtils : Singleton<SubnetUtils>
{
    using Singleton<SubnetUtils>::Singleton;

    [[nodiscard]] virtual Subnet random_subnet_from_range(Subnet::PrefixLength prefix = 24,
                                                          Subnet range = Subnet{
                                                              "10.0.0.0/8"}) const;
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
