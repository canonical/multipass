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

#ifndef MULTIPASS_CLIENT_PLATFORM_H
#define MULTIPASS_CLIENT_PLATFORM_H

#include <QString>

namespace multipass
{
namespace cli
{
namespace platform
{
void parse_copy_files_entry(const QString& entry, QString& path, QString& instance_name);
}
}
}
#endif // MULTIPASS_CLIENT_PLATFORM_H
