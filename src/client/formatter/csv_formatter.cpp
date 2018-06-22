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

#include <multipass/cli/csv_formatter.h>

#include <multipass/cli/format_utils.h>

#include <fmt/format.h>

namespace mp = multipass;

std::string mp::CSVFormatter::format(const InfoReply& reply) const
{
    fmt::memory_buffer buf;
    fmt::format_to(
        buf, "Name,State,Ipv4,Ipv6,Release,Image hash,Image release,Load,Disk usage,Disk total,Memory usage,Memory "
             "total,Mounts\n");

    for (const auto& info : reply.info())
    {
        fmt::format_to(buf, "{},{},{},{},{},{},{},{},{},{},{},{},", info.name(),
                       mp::format::status_string_for(info.instance_status()), info.ipv4(), info.ipv6(),
                       info.current_release(), info.id(), info.image_release(), info.load(), info.disk_usage(),
                       info.disk_total(), info.memory_usage(), info.memory_total());

        auto mount_paths = info.mount_info().mount_paths();
        for (auto mount = mount_paths.cbegin(); mount != mount_paths.cend(); ++mount)
        {
            fmt::format_to(buf, "{} => {};", mount->source_path(), mount->target_path());
        }

        fmt::format_to(buf, "\n");
    }
    return fmt::to_string(buf);
}

std::string mp::CSVFormatter::format(const ListReply& reply) const
{
    fmt::memory_buffer buf;

    fmt::format_to(buf, "Name,State,IPv4,IPv6,Release\n");

    for (const auto& instance : reply.instances())
    {
        fmt::format_to(buf, "{},{},{},{},{}\n", instance.name(),
                       mp::format::status_string_for(instance.instance_status()), instance.ipv4(), instance.ipv6(),
                       instance.current_release());
    }

    return fmt::to_string(buf);
}
