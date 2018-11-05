/*
 * Copyright (C) 2018 Canonical, Ltd.
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

#include "dhcp_release_process.h"

namespace mp = multipass;

mp::DHCPReleaseProcess::DHCPReleaseProcess(const mp::AppArmor& apparmor, const QString& bridge_name,
                                           const mp::IPAddress& ip, const QString& hw_addr)
    : mp::AppArmoredProcess(apparmor), bridge_name(bridge_name), ip(ip), hw_addr(hw_addr)
{
}

QString mp::DHCPReleaseProcess::program() const
{
    return QStringLiteral("dhcp_release");
}

QStringList mp::DHCPReleaseProcess::arguments() const
{
    return QStringList() << bridge_name << QString::fromStdString(ip.as_string()) << hw_addr;
}

QString mp::DHCPReleaseProcess::apparmor_profile() const
{
    return "TODO!";
}
