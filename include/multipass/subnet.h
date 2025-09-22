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

#define MP_SUBNET_UTILS multipass::SubnetUtils::instance();

namespace multipass
{
class Subnet
{
public:
    Subnet(IPAddress ip, uint8_t cidr);
    Subnet(const std::string& cidr_string);
    
    [[nodiscard]] IPAddress get_min_address() const;
    [[nodiscard]] IPAddress get_max_address() const;
    [[nodiscard]] uint32_t get_address_count() const;

    [[nodiscard]] IPAddress get_identifier() const;
    [[nodiscard]] uint8_t get_CIDR() const;
    [[nodiscard]] IPAddress get_subnet_mask() const;

    // uses CIDR notation
    [[nodiscard]] std::string as_string() const;

private:
    IPAddress identifier;
    uint8_t cidr;
};

struct SubnetUtils : Singleton<SubnetUtils>
{
    using Singleton<SubnetUtils>::Singleton;

    [[nodiscard]] virtual Subnet generate_random_subnet(IPAddress start, IPAddress end, uint8_t cidr) const;
    [[nodiscard]] virtual Subnet get_subnet(const Path& network_dir, const QString& bridge_name) const;
};
} // namespace multipass

