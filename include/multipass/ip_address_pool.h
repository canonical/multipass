/*
 * Copyright (C) 2017 Canonical, Ltd.
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
 * Authored by: Alberto Aguirre <alberto.aguirre@canonical.com>
 *
 */

#ifndef MULTIPASS_IP_ADDRESS_POOL_H
#define MULTIPASS_IP_ADDRESS_POOL_H

#include "ip_address.h"

#include <multipass/path.h>

#include <QDir>

#include <set>
#include <string>
#include <unordered_map>

namespace multipass
{
class IPAddressPool
{
public:
    IPAddressPool(const Path& data_dir, const IPAddress& start, const IPAddress& end);
    IPAddress obtain_ip_for(const std::string& name);
    void remove_ip_for(const std::string& name);

private:
    IPAddress obtain_free_ip();
    void persist_ips();
    const IPAddress start_ip;
    const IPAddress end_ip;
    const QDir data_dir;
    std::unordered_map<std::string, IPAddress> ip_map;
    std::set<IPAddress> ips_in_use;
};
}
#endif // MULTIPASS_IP_ADDRESS_POOL_H
