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

#include "common.h"

#include <multipass/network_interface.h>
#include <multipass/network_interface_info.h>

#include <QString>

#include <ostream>

QT_BEGIN_NAMESPACE
void PrintTo(const QString& qstr, std::ostream* os)
{
    *os << "QString(\"" << qUtf8Printable(qstr) << "\")";
}
QT_END_NAMESPACE

void multipass::PrintTo(const NetworkInterface& net, std::ostream* os)
{
    *os << "NetworkInterface(id=\"" << net.id << "\")";
}

void multipass::PrintTo(const NetworkInterfaceInfo& net, std::ostream* os)
{
    *os << "NetworkInterfaceInfo(id=\"" << net.id << "\")";
}
