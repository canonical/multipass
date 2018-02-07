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

#include <multipass/cli/format.h>
#include <multipass/cli/table_output.h>

#include <iomanip>
#include <iostream>
#include <sstream>

namespace mp = multipass;

namespace
{
std::ostream& operator<<(std::ostream& out, const mp::InfoReply::Info& info)
{
    std::string ipv4{info.ipv4()};
    std::string ipv6{info.ipv6()};
    if (ipv4.empty())
        ipv4.append("--");
    out << std::setw(16) << std::left << "Name:";
    out << info.name() << "\n";

    out << std::setw(16) << std::left << "State:";
    out << mp::format::status_string(info.instance_status()) << "\n";

    out << std::setw(16) << std::left << "IPv4:";
    out << ipv4 << "\n";

    if (!ipv6.empty())
    {
        out << std::setw(16) << std::left << "IPV6:";
        out << ipv6 << "\n";
    }

    out << std::setw(16) << std::left << "Release:";
    out << (info.current_release().empty() ? "--" : info.current_release()) << "\n";

    out << std::setw(16) << std::left << "Image hash:";
    out << info.id().substr(0, 12) << " (Ubuntu " << info.image_release() << ")\n";

    out << std::setw(16) << std::left << "Load:";
    out << (info.load().empty() ? "--" : info.load()) << "\n";

    out << std::setw(16) << std::left << "Disk usage:";
    out << (info.disk_usage().empty() ? "--" : info.disk_usage()) << "\n";

    out << std::setw(16) << std::left << "Memory usage:";
    out << (info.memory_usage().empty() ? "--" : info.memory_usage()) << "\n";

    auto mount_paths = info.mount_info().mount_paths();
    for (auto mount = mount_paths.cbegin(); mount != mount_paths.cend(); ++mount)
    {
        out << std::setw(16) << std::left << ((mount == mount_paths.cbegin()) ? "Mounts:" : " ")
            << std::setw(info.mount_info().longest_path_len()) << mount->source_path() << "  => "
            << mount->target_path() << "\n";
    }
    return out;
}

std::ostream& operator<<(std::ostream& out, const mp::ListVMInstance& instance)
{
    std::string ipv4{instance.ipv4()};
    std::string ipv6{instance.ipv6()};
    if (ipv4.empty())
        ipv4.append("--");
    if (ipv6.empty())
        ipv6.append("--");
    out << std::setw(24) << std::left << instance.name();
    out << std::setw(9) << std::left << mp::format::status_string(instance.instance_status());
    out << std::setw(17) << std::left << ipv4;
    out << instance.current_release();

    return out;
}
}

std::string mp::TableOutput::process_info(InfoReply& reply) const
{
    std::stringstream out;
    for (const auto& info : reply.info())
    {
        out << info << "\n";
    }
    auto output = out.str();
    if (!reply.info().empty())
        output.pop_back();

    return output;
}

std::string mp::TableOutput::process_list(ListReply& reply) const
{
    std::stringstream out;

    out << std::setw(24) << std::left << "Name";
    out << std::setw(9) << std::left << "State";
    out << std::setw(17) << std::left << "IPv4";
    out << std::left << "Release";
    out << "\n";

    for (const auto& instance : reply.instances())
    {
        out << instance << "\n";
    }

    return out.str();
}
