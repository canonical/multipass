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

#include <multipass/cli/client_platform.h>

#include <QFileInfo>

#include <unistd.h>

namespace mp = multipass;
namespace mcp = multipass::cli::platform;

void mcp::parse_transfer_entry(const QString& entry, QString& path, QString& instance_name)
{
    auto colon_count = entry.count(":");

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

std::string mcp::Platform::get_password(mp::Terminal*) const
{
    return {};
}

void mcp::Platform::enable_ansi_escape_chars() const
{
    return;
}
