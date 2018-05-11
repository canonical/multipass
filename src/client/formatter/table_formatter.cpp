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

#include <multipass/cli/table_formatter.h>

#include <multipass/cli/format_utils.h>

#include <fmt/format.h>

namespace mp = multipass;

namespace
{

auto human_readable_size(const std::string& num_in_bytes)
{
    auto bytes = std::stoll(num_in_bytes);

    if (bytes < 1024)
        return fmt::format("{}B", bytes);
    if (bytes < 1048576)
        return fmt::format("{:.1f}K", bytes / 1024.);
    if (bytes < 1073741824)
        return fmt::format("{:.1f}M", bytes / 1048576.);
    if (bytes < 1099511627776)
        return fmt::format("{:.1f}G", bytes / 1073741824.);

    return fmt::format("{:.1f}T", bytes / 1099511627776.);
}

std::string to_usage(const std::string& usage, const std::string& total)
{
    if (usage.empty() || total.empty())
        return "--";
    return fmt::format("{} out of {}", human_readable_size(usage), human_readable_size(total));
}

} // namespace
std::string mp::TableFormatter::format(const InfoReply& reply) const
{
    fmt::MemoryWriter out;

    for (const auto& info : reply.info())
    {
        out.write("{:<16}{}\n", "Name:", info.name());
        out.write("{:<16}{}\n", "State:", mp::format::status_string_for(info.instance_status()));
        out.write("{:<16}{}\n", "IPv4:", info.ipv4().empty() ? "--" : info.ipv4());

        if (!info.ipv6().empty())
        {
            out.write("{:<16}{}\n", "IPv6:", info.ipv6());
        }

        out.write("{:<16}{}\n", "Release:", info.current_release().empty() ? "--" : info.current_release());
        out.write("{:<16}", "Image hash:");
        if (info.id().empty())
            out.write("{}\n", "Not Available");
        else
            out.write("{}{}\n", info.id().substr(0, 12),
                      !info.image_release().empty() ? fmt::format(" (Ubuntu {})", info.image_release()) : "");
        out.write("{:<16}{}\n", "Load:", info.load().empty() ? "--" : info.load());
        out.write("{:<16}{}\n", "Disk usage:", to_usage(info.disk_usage(), info.disk_total()));
        out.write("{:<16}{}\n", "Memory usage:", to_usage(info.memory_usage(), info.memory_total()));

        auto mount_paths = info.mount_info().mount_paths();
        for (auto mount = mount_paths.cbegin(); mount != mount_paths.cend(); ++mount)
        {
            out.write("{:<16}{:{}} => {}\n", (mount == mount_paths.cbegin()) ? "Mounts:" : " ", mount->source_path(),
                      info.mount_info().longest_path_len(), mount->target_path());
        }

        out.write("\n");
    }
    auto output = out.str();
    if (!reply.info().empty())
        output.pop_back();
    else
        output = "\n";

    return output;
}

std::string mp::TableFormatter::format(const ListReply& reply) const
{
    fmt::MemoryWriter out;

    if (reply.instances().empty())
        return "No instances found.\n";

    out.write("{:<24}{:<12}{:<17}{:<}\n", "Name", "State", "IPv4", "Release");

    for (const auto& instance : reply.instances())
    {
        out.write("{:<24}{:<12}{:<17}{:<}\n", instance.name(),
                  mp::format::status_string_for(instance.instance_status()),
                  instance.ipv4().empty() ? "--" : instance.ipv4(),
                  instance.current_release().empty() ? "Not Available"
                                                     : fmt::format("Ubuntu {}", instance.current_release()));
    }

    return out.str();
}
