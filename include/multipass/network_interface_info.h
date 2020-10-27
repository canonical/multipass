/*
 * Copyright (C) 2020 Canonical, Ltd.
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

#ifndef MULTIPASS_NETWORK_INTERFACE_INFO_H
#define MULTIPASS_NETWORK_INTERFACE_INFO_H

#include <multipass/ip_address.h>
#include <multipass/optional.h>

namespace multipass
{
struct NetworkInterfaceInfo
{
    std::string id;
    std::string type;
    std::string description;
};
} // namespace multipass
#endif // MULTIPASS_NETWORK_INTERFACE_INFO_H
