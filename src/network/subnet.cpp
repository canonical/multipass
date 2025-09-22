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

namespace mp = multipass;

namespace
{
mp::Subnet parse(const std::string& cidr_string)
{
    return mp::Subnet(mp::IPAddress(""), 0);
}

mp::IPAddress apply_mask(mp::IPAddress ip, uint8_t cidr)
{
    return mp::IPAddress{""};
}
}

mp::Subnet::Subnet(IPAddress ip, uint8_t cidr) : identifier(apply_mask(ip, cidr)), cidr(cidr)
{
}

mp::Subnet::Subnet(const std::string& cidr_string) : Subnet(parse(cidr_string))
{
}

mp::IPAddress mp::Subnet::get_min_address() const
{
    return mp::IPAddress{""};
}

mp::IPAddress mp::Subnet::get_max_address() const
{
    return mp::IPAddress{""};
}

uint32_t mp::Subnet::get_address_count() const
{
    return 0;
}

mp::IPAddress mp::Subnet::get_identifier() const
{
    return mp::IPAddress{""};
}

uint8_t mp::Subnet::get_CIDR() const
{
    return 0;
}

mp::IPAddress mp::Subnet::get_subnet_mask() const
{
    return mp::IPAddress{""};
}

// uses CIDR notation
std::string mp::Subnet::as_string() const
{
    return "";
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
