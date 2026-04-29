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
#include <multipass/utils.h>
#include <multipass/utils/sorted_map_view.h>

namespace mp = multipass;

namespace
{
template <typename Dest>
void format_images(Dest&& dest,
                   const google::protobuf::RepeatedPtrField<mp::FindReply_ImageInfo>& images_info,
                   std::string type)
{
    for (const auto& image : images_info)
    {
        auto aliases = image.aliases();
        mp::format::filter_aliases(aliases);

        auto image_id = mp::format::image_string_for(image.remote_name(), aliases[0]);

        fmt::format_to(dest,
                       "{},{},{},{},{},{},{}\n",
                       image_id,
                       image.remote_name(),
                       fmt::join(aliases.cbegin() + 1, aliases.cend(), ";"),
                       image.os(),
                       image.release(),
                       image.version(),
                       type);
    }
}

std::string generate_snapshot_details(const mp::InfoReply reply)
{
    fmt::memory_buffer buf;

    fmt::format_to(
        std::back_inserter(buf),
        "Snapshot,Instance,CPU(s),Disk space,Memory size,Mounts,Created,Parent,Children,Comment\n");

    for (const auto& info : mp::format::sorted(reply.details()))
    {
        const auto& fundamentals = info.snapshot_info().fundamentals();

        fmt::format_to(std::back_inserter(buf),
                       "{},{},{},{},{},{},{},{},{},\"{}\"\n",
                       fundamentals.snapshot_name(),
                       info.name(),
                       info.cpu_count(),
                       info.disk_total(),
                       info.memory_total(),
                       info.mount_info(),
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
    fmt::format_to(std::back_inserter(buf),
                   "Name,State,"
#ifdef AVAILABILITY_ZONES_FEATURE
                   "Zone,Zone available,"
#endif
                   "Ipv4,Release,Image hash,Image release,Load,Disk usage,Disk "
                   "total,Memory usage,Memory "
                   "total,Mounts,AllIPv4,CPU(s){}\n",
                   have_num_snapshots ? ",Snapshots" : "");

    for (const auto& info : mp::format::sorted(reply.details()))
    {
        const auto& instance_details = info.instance_info();

        fmt::format_to(std::back_inserter(buf),
                       "{},{},"
#ifdef AVAILABILITY_ZONES_FEATURE
                       "{},{},"
#endif
                       "{},{},{},{},{},{},{},{},{},{},{},{}{}\n",
                       info.name(),
                       mp::format::status_string_for(info.instance_status()),
#ifdef AVAILABILITY_ZONES_FEATURE
                       info.zone().name(),
                       info.zone().available(),
#endif
                       instance_details.ipv4_size() ? instance_details.ipv4(0) : "",
                       instance_details.current_release(),
                       instance_details.id(),
                       instance_details.image_release(),
                       instance_details.load(),
                       instance_details.disk_usage(),
                       info.disk_total(),
                       instance_details.memory_usage(),
                       info.memory_total(),
                       info.mount_info(),
                       fmt::join(instance_details.ipv4(), ";"),
                       info.cpu_count(),
                       have_num_snapshots ? fmt::format(",{}", instance_details.num_snapshots())
                                          : "");
    }

    return fmt::to_string(buf);
}

std::string generate_instances_list(const mp::InstancesList& instance_list)
{
    fmt::memory_buffer buf;

    fmt::format_to(std::back_inserter(buf),
                   "Name,State,IPv4,Release,AllIPv4"
#ifdef AVAILABILITY_ZONES_FEATURE
                   ",Zone,Zone available"
#endif
                   "\n");

    for (const auto& instance : mp::format::sorted(instance_list.instances()))
    {
        fmt::format_to(
            std::back_inserter(buf),
            "{},{},{},{},\"{}\""
#ifdef AVAILABILITY_ZONES_FEATURE
            ",{},{}"
#endif
            "\n",
            instance.name(),
            mp::format::status_string_for(instance.instance_status()),
            instance.ipv4_size() ? instance.ipv4(0) : "",
            instance.current_release().empty()
                ? "Not Available"
                : mp::utils::trim(fmt::format("{} {}", instance.os(), instance.current_release())),
            fmt::join(instance.ipv4(), ",")
#ifdef AVAILABILITY_ZONES_FEATURE
                ,
            instance.zone().name(),
            instance.zone().available()
#endif
        );
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
        assert(reply.has_snapshot_list() &&
               "either one of instances or snapshots should be populated");
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
        fmt::format_to(std::back_inserter(buf),
                       "{},{},\"{}\"\n",
                       interface.name(),
                       interface.type(),
                       interface.description());
    }

    return fmt::to_string(buf);
}

std::string mp::CSVFormatter::format(const FindReply& reply) const
{
    fmt::memory_buffer buf;

    fmt::format_to(std::back_inserter(buf), "Image,Remote,Aliases,OS,Release,Version,Type\n");
    format_images(std::back_inserter(buf), reply.images_info(), "Cloud Image");

    return fmt::to_string(buf);
}

std::string mp::CSVFormatter::format(const VersionReply& reply,
                                     const std::string& client_version) const
{
    fmt::memory_buffer buf;

    fmt::format_to(std::back_inserter(buf), "Multipass,Multipassd,Title,Description,URL\n");

    fmt::format_to(std::back_inserter(buf),
                   "{},{},{},{},{}\n",
                   client_version,
                   reply.version(),
                   reply.update_info().title(),
                   reply.update_info().description(),
                   reply.update_info().url());

    return fmt::to_string(buf);
}

std::string mp::CSVFormatter::format(const mp::AliasDict& aliases) const
{
    fmt::memory_buffer buf;
    fmt::format_to(std::back_inserter(buf), "Alias,Instance,Command,Working directory,Context\n");

    for (const auto& [context_name, context_contents] : sorted_map_view(aliases))
    {
        std::string shown_context = context_name.get() == aliases.active_context_name()
                                        ? context_name.get() + "*"
                                        : context_name.get();

        for (const auto& [name, def] : sorted_map_view(context_contents.get()))
        {
            fmt::format_to(std::back_inserter(buf),
                           "{},{},{},{},{}\n",
                           name.get(),
                           def.get().instance,
                           def.get().command,
                           def.get().working_directory,
                           shown_context);
        }
    }

    return fmt::to_string(buf);
}

std::string mp::CSVFormatter::format(const ZonesReply& reply) const
{
    fmt::memory_buffer buf;
    fmt::format_to(std::back_inserter(buf), "Name,Available\n");

    for (const auto& zone : reply.zones())
    {
        fmt::format_to(std::back_inserter(buf), "{},{}\n", zone.name(), zone.available());
    }

    return fmt::to_string(buf);
}
