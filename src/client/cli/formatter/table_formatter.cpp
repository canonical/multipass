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

#include <regex>

namespace mp = multipass;

namespace
{
const std::regex newline("(\r\n|\n)");

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

std::string generate_snapshot_details(const mp::DetailedInfoItem& item)
{
    fmt::memory_buffer buf;
    const auto& fundamentals = item.snapshot_info().fundamentals();

    fmt::format_to(std::back_inserter(buf), "{:<16}{}\n", "Snapshot:", fundamentals.snapshot_name());
    fmt::format_to(std::back_inserter(buf), "{:<16}{}\n", "Instance:", item.name());

    if (!item.snapshot_info().size().empty())
        fmt::format_to(std::back_inserter(buf), "{:<16}{}\n", "Size:", item.snapshot_info().size());

    fmt::format_to(std::back_inserter(buf), "{:<16}{}\n", "CPU(s):", item.cpu_count());
    fmt::format_to(std::back_inserter(buf), "{:<16}{}\n", "Disk space:", item.disk_total());
    fmt::format_to(std::back_inserter(buf), "{:<16}{}\n", "Memory size:", item.memory_total());

    auto mount_paths = item.mount_info().mount_paths();
    fmt::format_to(std::back_inserter(buf), "{:<16}{}", "Mounts:", mount_paths.empty() ? "--\n" : "");
    for (auto mount = mount_paths.cbegin(); mount != mount_paths.cend(); ++mount)
    {
        if (mount != mount_paths.cbegin())
            fmt::format_to(std::back_inserter(buf), "{:<16}", "");
        fmt::format_to(std::back_inserter(buf),
                       "{:{}} => {}\n",
                       mount->source_path(),
                       item.mount_info().longest_path_len(),
                       mount->target_path());
    }

    fmt::format_to(std::back_inserter(buf),
                   "{:<16}{}\n",
                   "Created:",
                   MP_FORMAT_UTILS.convert_to_user_locale(fundamentals.creation_timestamp()));
    fmt::format_to(std::back_inserter(buf),
                   "{:<16}{}\n",
                   "Parent:",
                   fundamentals.parent().empty() ? "--" : fundamentals.parent());

    auto children = item.snapshot_info().children();
    fmt::format_to(std::back_inserter(buf), "{:<16}{}", "Children:", children.empty() ? "--\n" : "");
    for (auto child = children.cbegin(); child != children.cend(); ++child)
    {
        if (child != children.cbegin())
            fmt::format_to(std::back_inserter(buf), "{:<16}", "");
        fmt::format_to(std::back_inserter(buf), "{}\n", *child);
    }

    /* TODO split and align string if it extends onto several lines; but actually better implement generic word-wrapping
       for all output, taking both terminal width and current indentation level into account */
    fmt::format_to(std::back_inserter(buf),
                   "{:<16}{}\n",
                   "Comment:",
                   fundamentals.comment().empty()
                       ? "--"
                       : std::regex_replace(fundamentals.comment(), newline, "$&" + std::string(16, ' ')));

    return fmt::to_string(buf);
}

std::string generate_instance_details(const mp::DetailedInfoItem& item)
{
    fmt::memory_buffer buf;
    const auto& instance_details = item.instance_info();

    fmt::format_to(std::back_inserter(buf), "{:<16}{}\n", "Name:", item.name());
    fmt::format_to(std::back_inserter(buf),
                   "{:<16}{}\n",
                   "State:",
                   mp::format::status_string_for(item.instance_status()));

    if (instance_details.has_num_snapshots())
        fmt::format_to(std::back_inserter(buf), "{:<16}{}\n", "Snapshots:", instance_details.num_snapshots());

    int ipv4_size = instance_details.ipv4_size();
    fmt::format_to(std::back_inserter(buf), "{:<16}{}\n", "IPv4:", ipv4_size ? instance_details.ipv4(0) : "--");

    for (int i = 1; i < ipv4_size; ++i)
        fmt::format_to(std::back_inserter(buf), "{:<16}{}\n", "", instance_details.ipv4(i));

    if (int ipv6_size = instance_details.ipv6_size())
    {
        fmt::format_to(std::back_inserter(buf), "{:<16}{}\n", "IPv6:", instance_details.ipv6(0));

        for (int i = 1; i < ipv6_size; ++i)
            fmt::format_to(std::back_inserter(buf), "{:<16}{}\n", "", instance_details.ipv6(i));
    }

    fmt::format_to(std::back_inserter(buf),
                   "{:<16}{}\n",
                   "Release:",
                   instance_details.current_release().empty() ? "--" : instance_details.current_release());
    fmt::format_to(std::back_inserter(buf), "{:<16}", "Image hash:");
    if (instance_details.id().empty())
        fmt::format_to(std::back_inserter(buf), "{}\n", "Not Available");
    else
        fmt::format_to(std::back_inserter(buf),
                       "{}{}\n",
                       instance_details.id().substr(0, 12),
                       !instance_details.image_release().empty()
                           ? fmt::format(" (Ubuntu {})", instance_details.image_release())
                           : "");

    fmt::format_to(std::back_inserter(buf),
                   "{:<16}{}\n",
                   "CPU(s):",
                   item.cpu_count().empty() ? "--" : item.cpu_count());
    fmt::format_to(std::back_inserter(buf),
                   "{:<16}{}\n",
                   "Load:",
                   instance_details.load().empty() ? "--" : instance_details.load());
    fmt::format_to(std::back_inserter(buf),
                   "{:<16}{}\n",
                   "Disk usage:",
                   to_usage(instance_details.disk_usage(), item.disk_total()));
    fmt::format_to(std::back_inserter(buf),
                   "{:<16}{}\n",
                   "Memory usage:",
                   to_usage(instance_details.memory_usage(), item.memory_total()));

    const auto& mount_paths = item.mount_info().mount_paths();
    fmt::format_to(std::back_inserter(buf), "{:<16}{}", "Mounts:", mount_paths.empty() ? "--\n" : "");

    for (auto mount = mount_paths.cbegin(); mount != mount_paths.cend(); ++mount)
    {
        if (mount != mount_paths.cbegin())
            fmt::format_to(std::back_inserter(buf), "{:<16}", "");
        fmt::format_to(std::back_inserter(buf),
                       "{:{}} => {}\n",
                       mount->source_path(),
                       item.mount_info().longest_path_len(),
                       mount->target_path());

        auto mount_maps = mount->mount_maps();
        auto uid_mappings_size = mount_maps.uid_mappings_size();

        for (auto i = 0; i < uid_mappings_size; ++i)
        {
            auto uid_map_pair = mount_maps.uid_mappings(i);
            auto host_uid = uid_map_pair.host_id();
            auto instance_uid = uid_map_pair.instance_id();

            fmt::format_to(std::back_inserter(buf),
                           "{:>{}}{}:{}{}",
                           (i == 0) ? "UID map: " : "",
                           (i == 0) ? 29 : 0,
                           std::to_string(host_uid),
                           (instance_uid == mp::default_id) ? "default" : std::to_string(instance_uid),
                           (i == uid_mappings_size - 1) ? "\n" : ", ");
        }

        for (auto gid_mapping = mount_maps.gid_mappings().cbegin(); gid_mapping != mount_maps.gid_mappings().cend();
             ++gid_mapping)
        {
            auto host_gid = gid_mapping->host_id();
            auto instance_gid = gid_mapping->instance_id();

            fmt::format_to(std::back_inserter(buf),
                           "{:>{}}{}:{}{}{}",
                           (gid_mapping == mount_maps.gid_mappings().cbegin()) ? "GID map: " : "",
                           (gid_mapping == mount_maps.gid_mappings().cbegin()) ? 29 : 0,
                           std::to_string(host_gid),
                           (instance_gid == mp::default_id) ? "default" : std::to_string(instance_gid),
                           (std::next(gid_mapping) != mount_maps.gid_mappings().cend()) ? ", " : "",
                           (std::next(gid_mapping) == mount_maps.gid_mappings().cend()) ? "\n" : "");
        }
    }

    return fmt::to_string(buf);
}

std::string generate_instances_list(const mp::InstancesList& instance_list)
{
    fmt::memory_buffer buf;

    const auto& instances = instance_list.instances();

    if (instances.empty())
        return "No instances found.\n";

    const std::string name_col_header = "Name";
    const auto name_column_width = mp::format::column_width(
        instances.begin(),
        instances.end(),
        [](const auto& instance) -> int { return instance.name().length(); },
        name_col_header.length(),
        24);
    const std::string::size_type state_column_width = 18;
    const std::string::size_type ip_column_width = 17;

    const auto row_format = "{:<{}}{:<{}}{:<{}}{:<}\n";
    fmt::format_to(std::back_inserter(buf),
                   row_format,
                   name_col_header,
                   name_column_width,
                   "State",
                   state_column_width,
                   "IPv4",
                   ip_column_width,
                   "Image");

    for (const auto& instance : mp::format::sorted(instance_list.instances()))
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

std::string generate_snapshots_list(const mp::SnapshotsList& snapshot_list)
{
    fmt::memory_buffer buf;

    const auto& snapshots = snapshot_list.snapshots();

    if (snapshots.empty())
        return "No snapshots found.\n";

    const std::string name_col_header = "Instance", snapshot_col_header = "Snapshot", parent_col_header = "Parent",
                      comment_col_header = "Comment";
    const auto name_column_width = mp::format::column_width(
        snapshots.begin(),
        snapshots.end(),
        [](const auto& snapshot) -> int { return snapshot.name().length(); },
        name_col_header.length());
    const auto snapshot_column_width = mp::format::column_width(
        snapshots.begin(),
        snapshots.end(),
        [](const auto& snapshot) -> int { return snapshot.fundamentals().snapshot_name().length(); },
        snapshot_col_header.length());
    const auto parent_column_width = mp::format::column_width(
        snapshots.begin(),
        snapshots.end(),
        [](const auto& snapshot) -> int { return snapshot.fundamentals().parent().length(); },
        parent_col_header.length());

    const auto row_format = "{:<{}}{:<{}}{:<{}}{:<}\n";
    fmt::format_to(std::back_inserter(buf),
                   row_format,
                   name_col_header,
                   name_column_width,
                   snapshot_col_header,
                   snapshot_column_width,
                   parent_col_header,
                   parent_column_width,
                   comment_col_header);

    for (const auto& snapshot : mp::format::sorted(snapshot_list.snapshots()))
    {
        size_t max_comment_column_width = 50;
        std::smatch match;
        const auto& fundamentals = snapshot.fundamentals();

        if (std::regex_search(fundamentals.comment().begin(), fundamentals.comment().end(), match, newline))
            max_comment_column_width = std::min((size_t)(match.position(1)) + 1, max_comment_column_width);

        fmt::format_to(std::back_inserter(buf),
                       row_format,
                       snapshot.name(),
                       name_column_width,
                       fundamentals.snapshot_name(),
                       snapshot_column_width,
                       fundamentals.parent().empty() ? "--" : fundamentals.parent(),
                       parent_column_width,
                       fundamentals.comment().empty() ? "--"
                       : fundamentals.comment().length() > max_comment_column_width
                           ? fmt::format("{}…", fundamentals.comment().substr(0, max_comment_column_width - 1))
                           : fundamentals.comment());
    }

    return fmt::to_string(buf);
}
} // namespace

std::string mp::TableFormatter::format(const InfoReply& reply) const
{
    fmt::memory_buffer buf;

    for (const auto& info : mp::format::sorted(reply.details()))
    {
        if (info.has_instance_info())
        {
            fmt::format_to(std::back_inserter(buf), generate_instance_details(info));
        }
        else
        {
            assert(info.has_snapshot_info() && "either one of instance or snapshot details should be populated");
            fmt::format_to(std::back_inserter(buf), generate_snapshot_details(info));
        }

        fmt::format_to(std::back_inserter(buf), "\n");
    }

    std::string output = fmt::to_string(buf);
    if (!reply.details().empty())
        output.pop_back();
    else
        output = fmt::format("No {} found.\n", reply.snapshots() ? "snapshots" : "instances");

    return output;
}

std::string mp::TableFormatter::format(const ListReply& reply) const
{
    std::string output;

    if (reply.has_instance_list())
    {
        output = generate_instances_list(reply.instance_list());
    }
    else
    {
        assert(reply.has_snapshot_list() && "either one of instances or snapshots should be populated");
        output = generate_snapshots_list(reply.snapshot_list());
    }

    return output;
}

std::string mp::TableFormatter::format(const NetworksReply& reply) const
{
    fmt::memory_buffer buf;

    auto interfaces = reply.interfaces();

    if (interfaces.empty())
        return "No network interfaces found.\n";

    const std::string name_col_header = "Name", type_col_header = "Type", desc_col_header = "Description";
    const auto name_column_width = mp::format::column_width(
        interfaces.begin(),
        interfaces.end(),
        [](const auto& interface) -> int { return interface.name().length(); },
        name_col_header.length());
    const auto type_column_width = mp::format::column_width(
        interfaces.begin(),
        interfaces.end(),
        [](const auto& interface) -> int { return interface.type().length(); },
        type_col_header.length());

    const auto row_format = "{:<{}}{:<{}}{:<}\n";
    fmt::format_to(std::back_inserter(buf),
                   row_format,
                   name_col_header,
                   name_column_width,
                   type_col_header,
                   type_column_width,
                   desc_col_header);

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

    auto width = [&aliases](const auto get_width, int header_width) -> int {
        return mp::format::column_width(
            aliases.cbegin(),
            aliases.cend(),
            [&, get_width](const auto& ctx) -> int {
                return ctx.second.empty() ? 0
                                          : get_width(*std::max_element(ctx.second.cbegin(),
                                                                        ctx.second.cend(),
                                                                        [&get_width](const auto& lhs, const auto& rhs) {
                                                                            return get_width(lhs) < get_width(rhs);
                                                                        }));
            },
            header_width);
    };

    const std::string alias_col_header = "Alias", instance_col_header = "Instance", command_col_header = "Command",
                      context_col_header = "Context", dir_col_header = "Working directory";
    const std::string active_context = "*";
    const auto alias_width =
        width([](const auto& alias) -> int { return alias.first.length(); }, alias_col_header.length());
    const auto instance_width =
        width([](const auto& alias) -> int { return alias.second.instance.length(); }, instance_col_header.length());
    const auto command_width =
        width([](const auto& alias) -> int { return alias.second.command.length(); }, command_col_header.length());
    const auto context_width = mp::format::column_width(
        aliases.cbegin(),
        aliases.cend(),
        [&aliases, &active_context](const auto& alias) -> int {
            return alias.first == aliases.active_context_name() && !aliases.get_active_context().empty()
                       ? alias.first.length() + active_context.length()
                       : alias.first.length();
        },
        context_col_header.length());

    const auto row_format = "{:<{}}{:<{}}{:<{}}{:<{}}{:<}\n";

    fmt::format_to(std::back_inserter(buf),
                   row_format,
                   alias_col_header,
                   alias_width,
                   instance_col_header,
                   instance_width,
                   command_col_header,
                   command_width,
                   context_col_header,
                   context_width,
                   dir_col_header);

    for (const auto& [context_name, context_contents] : sort_dict(aliases))
    {
        std::string shown_context =
            context_name == aliases.active_context_name() ? context_name + active_context : context_name;

        for (const auto& [name, def] : sort_dict(context_contents))
        {
            fmt::format_to(std::back_inserter(buf), row_format, name, alias_width, def.instance, instance_width,
                           def.command, command_width, shown_context, context_width, def.working_directory);
        }
    }

    return fmt::to_string(buf);
}
