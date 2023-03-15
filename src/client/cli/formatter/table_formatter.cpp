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

#include <multipass/cli/alias_dict.h>
#include <multipass/cli/client_common.h>
#include <multipass/cli/format_utils.h>
#include <multipass/cli/table_formatter.h>
#include <multipass/format.h>
#include <multipass/memory_size.h>

namespace mp = multipass;

namespace
{
std::string format_images(const google::protobuf::RepeatedPtrField<mp::FindReply_ImageInfo>& images_info,
                          std::string type)
{
    fmt::memory_buffer buf;

    fmt::format_to(std::back_inserter(buf), "{:<28}{:<18}{:<17}{:<}\n", type, "Aliases", "Version", "Description");
    for (const auto& image : images_info)
    {
        auto aliases = image.aliases_info();
        mp::format::filter_aliases(aliases);

        fmt::format_to(std::back_inserter(buf), "{:<28}{:<18}{:<17}{:<}\n", mp::format::image_string_for(aliases[0]),
                       fmt::format("{}", fmt::join(aliases.cbegin() + 1, aliases.cend(), ",")), image.version(),
                       fmt::format("{}{}", image.os().empty() ? "" : image.os() + " ", image.release()));
    }
    fmt::format_to(std::back_inserter(buf), "\n");

    return fmt::to_string(buf);
}

std::string to_usage(const std::string& usage, const std::string& total)
{
    if (usage.empty() || total.empty())
        return "--";
    return fmt::format("{} out of {}", mp::MemorySize{usage}.human_readable(), mp::MemorySize{total}.human_readable());
}

} // namespace
std::string mp::TableFormatter::format(const InfoReply& reply) const
{
    fmt::memory_buffer buf;

    for (const auto& info : format::sorted(reply.info()))
    {
        fmt::format_to(std::back_inserter(buf), "{:<16}{}\n", "Name:", info.name());
        fmt::format_to(std::back_inserter(buf), "{:<16}{}\n",
                       "State:", mp::format::status_string_for(info.instance_status()));

        int ipv4_size = info.ipv4_size();
        fmt::format_to(std::back_inserter(buf), "{:<16}{}\n", "IPv4:", ipv4_size ? info.ipv4(0) : "--");

        for (int i = 1; i < ipv4_size; ++i)
            fmt::format_to(std::back_inserter(buf), "{:<16}{}\n", "", info.ipv4(i));

        if (int ipv6_size = info.ipv6_size())
        {
            fmt::format_to(std::back_inserter(buf), "{:<16}{}\n", "IPv6:", info.ipv6(0));

            for (int i = 1; i < ipv6_size; ++i)
                fmt::format_to(std::back_inserter(buf), "{:<16}{}\n", "", info.ipv6(i));
        }

        fmt::format_to(std::back_inserter(buf), "{:<16}{}\n",
                       "Release:", info.current_release().empty() ? "--" : info.current_release());
        fmt::format_to(std::back_inserter(buf), "{:<16}", "Image hash:");
        if (info.id().empty())
            fmt::format_to(std::back_inserter(buf), "{}\n", "Not Available");
        else
            fmt::format_to(std::back_inserter(buf), "{}{}\n", info.id().substr(0, 12),
                           !info.image_release().empty() ? fmt::format(" (Ubuntu {})", info.image_release()) : "");

        fmt::format_to(std::back_inserter(buf), "{:<16}{}\n",
                       "CPU(s):", info.cpu_count().empty() ? "--" : info.cpu_count());
        fmt::format_to(std::back_inserter(buf), "{:<16}{}\n", "Load:", info.load().empty() ? "--" : info.load());
        fmt::format_to(std::back_inserter(buf), "{:<16}{}\n",
                       "Disk usage:", to_usage(info.disk_usage(), info.disk_total()));
        fmt::format_to(std::back_inserter(buf), "{:<16}{}\n",
                       "Memory usage:", to_usage(info.memory_usage(), info.memory_total()));

        auto mount_paths = info.mount_info().mount_paths();
        fmt::format_to(std::back_inserter(buf), "{:<16}{}", "Mounts:", mount_paths.empty() ? "--\n" : "");

        for (auto mount = mount_paths.cbegin(); mount != mount_paths.cend(); ++mount)
        {
            if (mount != mount_paths.cbegin())
                fmt::format_to(std::back_inserter(buf), "{:<16}", "");
            fmt::format_to(std::back_inserter(buf), "{:{}} => {}\n", mount->source_path(),
                           info.mount_info().longest_path_len(), mount->target_path());

            auto mount_maps = mount->mount_maps();
            auto uid_mappings_size = mount_maps.uid_mappings_size();

            for (auto i = 0; i < uid_mappings_size; ++i)
            {
                auto uid_map_pair = mount_maps.uid_mappings(i);
                auto host_uid = uid_map_pair.host_id();
                auto instance_uid = uid_map_pair.instance_id();

                fmt::format_to(std::back_inserter(buf), "{:>{}}{}:{}{}", (i == 0) ? "UID map: " : "", (i == 0) ? 29 : 0,
                               std::to_string(host_uid),
                               (instance_uid == mp::default_id) ? "default" : std::to_string(instance_uid),
                               (i == uid_mappings_size - 1) ? "\n" : ", ");
            }

            for (auto gid_mapping = mount_maps.gid_mappings().cbegin(); gid_mapping != mount_maps.gid_mappings().cend();
                 ++gid_mapping)
            {
                auto host_gid = gid_mapping->host_id();
                auto instance_gid = gid_mapping->instance_id();

                fmt::format_to(std::back_inserter(buf), "{:>{}}{}:{}{}{}",
                               (gid_mapping == mount_maps.gid_mappings().cbegin()) ? "GID map: " : "",
                               (gid_mapping == mount_maps.gid_mappings().cbegin()) ? 29 : 0, std::to_string(host_gid),
                               (instance_gid == mp::default_id) ? "default" : std::to_string(instance_gid),
                               (std::next(gid_mapping) != mount_maps.gid_mappings().cend()) ? ", " : "",
                               (std::next(gid_mapping) == mount_maps.gid_mappings().cend()) ? "\n" : "");
            }
        }

        fmt::format_to(std::back_inserter(buf), "\n");
    }

    auto output = fmt::to_string(buf);
    if (!reply.info().empty())
        output.pop_back();
    else
        output = "\n";

    return output;
}

std::string mp::TableFormatter::format(const ListReply& reply) const
{
    fmt::memory_buffer buf;

    auto instances = reply.instances();

    if (instances.empty())
        return "No instances found.\n";

    const auto name_column_width = mp::format::column_width(
        instances.begin(), instances.end(), [](const auto& instance) -> int { return instance.name().length(); }, 24);
    const std::string::size_type state_column_width = 18;
    const std::string::size_type ip_column_width = 17;

    const auto row_format = "{:<{}}{:<{}}{:<{}}{:<}\n";
    fmt::format_to(std::back_inserter(buf), row_format, "Name", name_column_width, "State", state_column_width, "IPv4",
                   ip_column_width, "Image");

    for (const auto& instance : format::sorted(reply.instances()))
    {
        int ipv4_size = instance.ipv4_size();

        fmt::format_to(std::back_inserter(buf), row_format, instance.name(), name_column_width,
                       mp::format::status_string_for(instance.instance_status()), state_column_width,
                       ipv4_size ? instance.ipv4(0) : "--", ip_column_width,
                       instance.current_release().empty() ? "Not Available"
                                                          : fmt::format("Ubuntu {}", instance.current_release()));

        for (int i = 1; i < ipv4_size; ++i)
        {
            fmt::format_to(std::back_inserter(buf), row_format, "", name_column_width, "", state_column_width,
                           instance.ipv4(i), instance.ipv4(i).size(), "");
        }
    }

    return fmt::to_string(buf);
}

std::string mp::TableFormatter::format(const NetworksReply& reply) const
{
    fmt::memory_buffer buf;

    auto interfaces = reply.interfaces();

    if (interfaces.empty())
        return "No network interfaces found.\n";

    const auto name_column_width = mp::format::column_width(
        interfaces.begin(), interfaces.end(), [](const auto& interface) -> int { return interface.name().length(); },
        5);

    const auto type_column_width = mp::format::column_width(
        interfaces.begin(), interfaces.end(), [](const auto& interface) -> int { return interface.type().length(); },
        5);

    const auto row_format = "{:<{}}{:<{}}{:<}\n";
    fmt::format_to(std::back_inserter(buf), row_format, "Name", name_column_width, "Type", type_column_width,
                   "Description");

    for (const auto& interface : format::sorted(reply.interfaces()))
    {
        fmt::format_to(std::back_inserter(buf), row_format, interface.name(), name_column_width, interface.type(),
                       type_column_width, interface.description());
    }

    return fmt::to_string(buf);
}

std::string mp::TableFormatter::format(const FindReply& reply) const
{
    fmt::memory_buffer buf;

    if (reply.show_images() && reply.show_blueprints())
    {
        if (reply.images_info().empty() && reply.blueprints_info().empty())
        {
            fmt::format_to(std::back_inserter(buf), "No images or blueprints found.\n");
        }
        else
        {
            if (!reply.images_info().empty())
                fmt::format_to(std::back_inserter(buf), format_images(reply.images_info(), "Image"));

            if (!reply.blueprints_info().empty())
                fmt::format_to(std::back_inserter(buf), format_images(reply.blueprints_info(), "Blueprint"));
        }
    }
    else if (reply.show_images() && !reply.show_blueprints())
    {
        if (reply.images_info().empty())
            fmt::format_to(std::back_inserter(buf), "No images found.\n");
        else
            fmt::format_to(std::back_inserter(buf), format_images(reply.images_info(), "Image"));
    }
    else if (!reply.show_images() && reply.show_blueprints())
    {
        if (reply.blueprints_info().empty())
            fmt::format_to(std::back_inserter(buf), "No blueprints found.\n");
        else
            fmt::format_to(std::back_inserter(buf), format_images(reply.blueprints_info(), "Blueprint"));
    }

    return fmt::to_string(buf);
}

std::string mp::TableFormatter::format(const VersionReply& reply, const std::string& client_version) const
{
    fmt::memory_buffer buf;
    fmt::format_to(std::back_inserter(buf), "{:<12}{}\n", "multipass", client_version);

    if (!reply.version().empty())
    {
        fmt::format_to(std::back_inserter(buf), "{:<12}{}\n", "multipassd", reply.version());

        if (mp::cmd::update_available(reply.update_info()))
        {
            fmt::format_to(std::back_inserter(buf), "{}", mp::cmd::update_notice(reply.update_info()));
        }
    }
    return fmt::to_string(buf);
}

std::string mp::TableFormatter::format(const mp::AliasDict& aliases) const
{
    fmt::memory_buffer buf;

    if (aliases.empty())
        return "No aliases defined.\n";

    const auto alias_width = mp::format::column_width(
        aliases.cbegin(), aliases.cend(), [](const auto& alias) -> int { return alias.first.length(); }, 7);
    const auto instance_width = mp::format::column_width(
        aliases.cbegin(), aliases.cend(), [](const auto& alias) -> int { return alias.second.instance.length(); }, 10);
    const auto command_width = mp::format::column_width(
        aliases.cbegin(), aliases.cend(), [](const auto& alias) -> int { return alias.second.command.length(); }, 9);

    const auto row_format = "{:<{}}{:<{}}{:<{}}{:<}\n";

    fmt::format_to(std::back_inserter(buf), row_format, "Alias", alias_width, "Instance", instance_width, "Command",
                   command_width, "Working directory");

    for (const auto& elt : sort_dict(aliases))
    {
        const auto& name = elt.first;
        const auto& def = elt.second;

        fmt::format_to(std::back_inserter(buf), row_format, name, alias_width, def.instance, instance_width,
                       def.command, command_width, def.working_directory);
    }

    return fmt::to_string(buf);
}
