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

#ifndef MULTIPASS_NETWORK_INTERFACE_H
#define MULTIPASS_NETWORK_INTERFACE_H

#include <string>

namespace multipass
{
struct NetworkInterface
{
    std::string id;
    std::string mac_address;
    bool auto_mode;
};
} // namespace multipass

#endif // MULTIPASS_NETWORK_INTERFACE_H
