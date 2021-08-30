/*
 * Copyright (C) 2018-2021 Canonical, Ltd.
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
#include <multipass/format.h>

#include <QFileInfo>

#include <cassert>
#include <unistd.h>

namespace mcp = multipass::cli::platform;

void mcp::parse_transfer_entry(const QString& entry, QString& path, QString& instance_name)
{
    auto colon_count = entry.count(":");
    assert(colon_count >= 0);

    switch (colon_count)
    {
    case 0:
        path = entry;
        break;
    case 1:
        if (!QFileInfo::exists(entry))
        {
            instance_name = entry.section(":", 0, 0);
            path = entry.section(":", 1);
        }
        else
        {
            path = entry;
        }
        break;
    default:
        throw std::runtime_error{fmt::format("Too many colons in \"{}\"", entry)};
    }
}

int mcp::getuid()
{
    return ::getuid();
}

int mcp::getgid()
{
    return ::getgid();
}
