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

#ifndef MULTIPASS_PLATFORM_LINUX_DETAIL_H
#define MULTIPASS_PLATFORM_LINUX_DETAIL_H

#include <multipass/network_interface_info.h>

#include <map>

namespace multipass::platform::detail
{
std::map<std::string, NetworkInterfaceInfo> get_network_interfaces_from(const QDir& sys_dir);
std::unique_ptr<QFile> find_os_release();
std::pair<QString, QString> parse_os_release(const QStringList& os_data);
std::string read_os_release();
} // namespace multipass::platform::detail

#endif // MULTIPASS_PLATFORM_LINUX_DETAIL_H
