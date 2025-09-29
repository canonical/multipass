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

#include <multipass/path.h>
#include <multipass/singleton.h>

#include "ip_address.h"

#define MP_SUBNET_UTILS multipass::SubnetUtils::instance()

namespace multipass
{
class Subnet
{
public:
    Subnet(IPAddress ip, uint8_t cidr);
    Subnet(const std::string& cidr_string);
    
    [[nodiscard]] IPAddress min_address() const;
    [[nodiscard]] IPAddress max_address() const;
    [[nodiscard]] uint32_t address_count() const;

    [[nodiscard]] IPAddress identifier() const;
    [[nodiscard]] uint8_t CIDR() const;
    [[nodiscard]] IPAddress subnet_mask() const;

    // uses CIDR notation
    [[nodiscard]] std::string as_string() const;

    // Subnets are either disjoint or the smaller is a subset of the larger
    [[nodiscard]] bool contains(Subnet other) const;
    [[nodiscard]] bool contains(IPAddress ip) const;

    [[nodiscard]] bool operator==(const Subnet& other) const;
    [[nodiscard]] bool operator<(const Subnet& other) const;
private:
    IPAddress id;
    uint8_t cidr;
};

struct SubnetUtils : Singleton<SubnetUtils>
{
    using Singleton<SubnetUtils>::Singleton;

    [[nodiscard]] virtual Subnet generate_random_subnet(uint8_t cidr = 24,
                                                        Subnet range = Subnet{"10.0.0.0/8"}) const;
};
} // namespace multipass

