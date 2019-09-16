/*
 * Copyright (C) 2019 Canonical, Ltd.
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

#ifndef MULTIPASS_IPTABLES_CONFIG_H
#define MULTIPASS_IPTABLES_CONFIG_H

#include <QString>

namespace multipass
{
class IPTablesConfig
{
public:
    IPTablesConfig(const QString& bridge_name, const std::string& subnet);
    ~IPTablesConfig();

    void verify_iptables_rules();

private:
    void clear_all_iptables_rules();

    const QString bridge_name;
    const QString cidr;
    const QString comment;
};
} // namespace multipass
#endif // MULTIPASS_IPTABLES_CONFIG_H
