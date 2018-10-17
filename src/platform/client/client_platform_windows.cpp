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

#include <multipass/cli/client_platform.h>

#include <QFileInfo>

#include <io.h>

namespace mp = multipass;
namespace mcp = multipass::cli::platform;

void mcp::parse_copy_files_entry(const QString& entry, QString& path, QString& instance_name)
{
    auto colon_count = entry.count(":");

    switch (colon_count)
    {
    case 0:
        path = entry;
        break;
    case 1:
        if (!QFileInfo::exists(entry) && !entry.contains(":\\"))
        {
            instance_name = entry.section(":", 0, 0);
            path = entry.section(":", 1);
        }
        else
        {
            path = entry;
        }
        break;
    }
}

bool mcp::is_tty()
{
    return !(_isatty(_fileno(stdin)) == 0);
}

int mcp::getuid()
{
    return mp::no_id_info_available;
}

int mcp::getgid()
{
    return mp::no_id_info_available;
}
