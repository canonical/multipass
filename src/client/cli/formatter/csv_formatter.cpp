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

#include <multipass/cli/csv_formatter.h>
#include <multipass/cli/format_utils.h>
#include <multipass/format.h>

namespace mp = multipass;

namespace
{
std::string format_images(const google::protobuf::RepeatedPtrField<mp::FindReply_ImageInfo>& images_info,
                          std::string type)
{
    fmt::memory_buffer buf;

    for (const auto& image : images_info)
    {
        auto aliases = image.aliases_info();

        mp::format::filter_aliases(aliases);

        auto image_id = aliases[0].remote_name().empty()
                            ? aliases[0].alias()
                            : fmt::format("{}:{}", aliases[0].remote_name(), aliases[0].alias());

        fmt::format_to(std::back_inserter(buf), "{},{},{},{},{},{},{}\n", image_id, aliases[0].remote_name(),
                       fmt::join(aliases.cbegin() + 1, aliases.cend(), ";"), image.os(), image.release(),
                       image.version(), type);
    }

    return fmt::to_string(buf);
}

std::string format_mounts(const mp::MountInfo& mount_info)
{
    fmt::memory_buffer buf;
    const auto& mount_paths = mount_info.mount_paths();

    if (!mount_paths.size())
        return {};

    auto mount = mount_paths.cbegin();
    for (; mount != --mount_paths.cend(); ++mount)
        fmt::format_to(std::back_inserter(buf), "{} => {};", mount->source_path(), mount->target_path());
    fmt::format_to(std::back_inserter(buf), "{} => {}", mount->source_path(), mount->target_path());

    return fmt::to_string(buf);
}

std::string generate_snapshot_details(const mp::InfoReply reply)
{
    fmt::memory_buffer buf;

    fmt::format_to(std::back_inserter(buf),
                   "Snapshot,Instance,CPU(s),Disk space,Memory size,Mounts,Created,Parent,Children,Comment\n");

    for (const auto& info : mp::format::sorted(reply.details()))
    {
        const auto& fundamentals = info.snapshot_info().fundamentals();

        fmt::format_to(std::back_inserter(buf),
                       "{},{},{},{},{},",
                       fundamentals.snapshot_name(),
                       info.name(),
                       info.cpu_count(),
                       info.disk_total(),
                       info.memory_total());

        fmt::format_to(std::back_inserter(buf), format_mounts(info.mount_info()));

        fmt::format_to(std::back_inserter(buf),
                       ",{},{},{},\"{}\"\n",
                       MP_FORMAT_UTILS.convert_to_user_locale(fundamentals.creation_timestamp()),
                       fundamentals.parent(),
                       fmt::join(info.snapshot_info().children(), ";"),
                       fundamentals.comment());
    }

    return fmt::to_string(buf);
}

std::string generate_instance_details(const mp::InfoReply reply)
{
    assert(reply.details_size() && "shouldn't call this if there are not entries");
    assert(reply.details(0).has_instance_info() &&
           "outputting instance and snapshot details together is not supported in csv format");

    auto have_num_snapshots = reply.details(0).instance_info().has_num_snapshots();

    fmt::memory_buffer buf;
    fmt::format_to(
        std::back_inserter(buf),
        "Name,State,Ipv4,Ipv6,Release,Image hash,Image release,Load,Disk usage,Disk total,Memory usage,Memory "
        "total,Mounts,AllIPv4,CPU(s){}\n",
        have_num_snapshots ? ",Snapshots" : "");

    for (const auto& info : mp::format::sorted(reply.details()))
    {
        const auto& instance_details = info.instance_info();

        fmt::format_to(std::back_inserter(buf),
                       "{},{},{},{},{},{},{},{},{},{},{},{},",
                       info.name(),
                       mp::format::status_string_for(info.instance_status()),
                       instance_details.ipv4_size() ? instance_details.ipv4(0) : "",
                       instance_details.ipv6_size() ? instance_details.ipv6(0) : "",
                       instance_details.current_release(),
                       instance_details.id(),
                       instance_details.image_release(),
                       instance_details.load(),
                       instance_details.disk_usage(),
                       info.disk_total(),
                       instance_details.memory_usage(),
                       info.memory_total());

        fmt::format_to(std::back_inserter(buf), format_mounts(info.mount_info()));

        fmt::format_to(std::back_inserter(buf), ",{},{}", fmt::join(instance_details.ipv4(), ";"), info.cpu_count());

        fmt::format_to(std::back_inserter(buf),
                       "{}\n",
                       have_num_snapshots ? fmt::format(",{}", instance_details.num_snapshots()) : "");
    }

    return fmt::to_string(buf);
}

std::string generate_instances_list(const mp::InstancesList& instance_list)
{
    fmt::memory_buffer buf;

    fmt::format_to(std::back_inserter(buf), "Name,State,IPv4,IPv6,Release,AllIPv4\n");

    for (const auto& instance : mp::format::sorted(instance_list.instances()))
    {
        fmt::format_to(std::back_inserter(buf), "{},{},{},{},{},\"{}\"\n", instance.name(),
                       mp::format::status_string_for(instance.instance_status()),
                       instance.ipv4_size() ? instance.ipv4(0) : "", instance.ipv6_size() ? instance.ipv6(0) : "",
                       instance.current_release().empty() ? "Not Available"
                                                          : fmt::format("Ubuntu {}", instance.current_release()),
                       fmt::join(instance.ipv4(), ","));
    }

    return fmt::to_string(buf);
}

std::string generate_snapshots_list(const mp::SnapshotsList& snapshot_list)
{
    fmt::memory_buffer buf;

    fmt::format_to(std::back_inserter(buf), "Instance,Snapshot,Parent,Comment\n");

    for (const auto& item : mp::format::sorted(snapshot_list.snapshots()))
    {
        const auto& snapshot = item.fundamentals();
        fmt::format_to(std::back_inserter(buf),
                       "{},{},{},\"{}\"\n",
                       item.name(),
                       snapshot.snapshot_name(),
                       snapshot.parent(),
                       snapshot.comment());
    }

    return fmt::to_string(buf);
}
} // namespace

std::string mp::CSVFormatter::format(const InfoReply& reply) const
{
    std::string output;

    if (reply.details_size() > 0)
    {
        if (reply.details()[0].has_instance_info())
            output = generate_instance_details(reply);
        else
            output = generate_snapshot_details(reply);
    }

    return output;
}

std::string mp::CSVFormatter::format(const ListReply& reply) const
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

std::string mp::CSVFormatter::format(const NetworksReply& reply) const
{
    fmt::memory_buffer buf;

    fmt::format_to(std::back_inserter(buf), "Name,Type,Description\n");

    for (const auto& interface : format::sorted(reply.interfaces()))
    {
        // Quote the description because it can contain commas.
        fmt::format_to(std::back_inserter(buf), "{},{},\"{}\"\n", interface.name(), interface.type(),
                       interface.description());
    }

    return fmt::to_string(buf);
}

std::string mp::CSVFormatter::format(const FindReply& reply) const
{
    fmt::memory_buffer buf;

    fmt::format_to(std::back_inserter(buf), "Image,Remote,Aliases,OS,Release,Version,Type\n");
    fmt::format_to(std::back_inserter(buf), format_images(reply.images_info(), "Cloud Image"));
    fmt::format_to(std::back_inserter(buf), format_images(reply.blueprints_info(), "Blueprint"));

    return fmt::to_string(buf);
}

std::string mp::CSVFormatter::format(const VersionReply& reply, const std::string& client_version) const
{
    fmt::memory_buffer buf;

    fmt::format_to(std::back_inserter(buf), "Multipass,Multipassd,Title,Description,URL\n");

    fmt::format_to(std::back_inserter(buf), "{},{},{},{},{}\n", client_version, reply.version(),
                   reply.update_info().title(), reply.update_info().description(), reply.update_info().url());

    return fmt::to_string(buf);
}

std::string mp::CSVFormatter::format(const mp::AliasDict& aliases) const
{
    fmt::memory_buffer buf;
    fmt::format_to(std::back_inserter(buf), "Alias,Instance,Command,Working directory,Context\n");

    for (const auto& [context_name, context_contents] : sort_dict(aliases))
    {
        std::string shown_context = context_name == aliases.active_context_name() ? context_name + "*" : context_name;

        for (const auto& [name, def] : sort_dict(context_contents))
        {
            fmt::format_to(std::back_inserter(buf), "{},{},{},{},{}\n", name, def.instance, def.command,
                           def.working_directory, shown_context);
        }
    }

    return fmt::to_string(buf);
}
