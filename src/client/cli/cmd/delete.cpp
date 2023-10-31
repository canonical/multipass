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

#include "delete.h"
#include "common_cli.h"

#include <multipass/cli/argparser.h>
#include <multipass/platform.h>

namespace mp = multipass;
namespace cmd = multipass::cmd;

namespace
{
constexpr auto no_purge_base_error_msg =
    "Snapshots can only be purged (after deletion, they cannot be recovered). Please use the `--purge` "
    "flag if that is what you want";
}

mp::ReturnCode cmd::Delete::run(mp::ArgParser* parser)
{
    auto ret = parse_args(parser);
    if (ret != ParseCode::Ok)
    {
        return parser->returnCodeFrom(ret);
    }

    auto on_success = [this](mp::DeleteReply& reply) {
        auto size = reply.purged_instances_size();
        for (auto i = 0; i < size; ++i)
        {
            const auto purged_instance = reply.purged_instances(i);
            const auto removed_aliases = aliases.remove_aliases_for_instance(purged_instance);

            for (const auto& removed_alias : removed_aliases)
            {
                const auto& [removal_context, removed_alias_name] = removed_alias;
                try
                {
                    MP_PLATFORM.remove_alias_script(removal_context + "." + removed_alias_name);

                    if (!aliases.exists_alias(removed_alias_name))
                    {
                        MP_PLATFORM.remove_alias_script(removed_alias_name);
                    }
                }
                catch (const std::runtime_error& e)
                {
                    cerr << fmt::format("Warning: '{}' when removing alias script for {}.{}\n", e.what(),
                                        removal_context, removed_alias_name);
                }
            }
        }

        return mp::ReturnCode::Ok;
    };

    auto on_failure = [this](grpc::Status& status) { return standard_failure_handler_for(name(), cerr, status); };

    request.set_verbosity_level(parser->verbosityLevel());
    return dispatch(&RpcMethod::delet, request, on_success, on_failure);
}

std::string cmd::Delete::name() const
{
    return "delete";
}

QString cmd::Delete::short_help() const
{
    return QStringLiteral("Delete instances and snapshots");
}

QString cmd::Delete::description() const
{
    return QStringLiteral(
        "Delete instances and snapshots. Instances can be purged immediately or later on,\n"
        "with the \"purge\" command. Until they are purged, instances can be recovered\n"
        "with the \"recover\" command. Snapshots cannot be recovered after deletion and must be purged at once.");
}

mp::ParseCode cmd::Delete::parse_args(mp::ArgParser* parser)
{
    parser->addPositionalArgument("name",
                                  "Names of instances and snapshots to delete",
                                  "<instance>[.snapshot] [<instance>[.snapshot] ...]");

    QCommandLineOption all_option(all_option_name, "Delete all instances and snapshots");
    QCommandLineOption purge_option({"p", "purge"}, "Permanently delete specified instances and snapshots immediately");
    parser->addOptions({all_option, purge_option});

    auto status = parser->commandParse(this);
    if (status != ParseCode::Ok)
        return status;

    status = check_for_name_and_all_option_conflict(parser, cerr);
    if (status != ParseCode::Ok)
        return status;

    request.set_purge(parser->isSet(purge_option));

    status = parse_instances_snapshots(parser);

    return status;
}

mp::ParseCode cmd::Delete::parse_instances_snapshots(mp::ArgParser* parser)
{
    bool instance_found = false, snapshot_found = false;
    std::string instances, snapshots;
    for (const auto& item : cmd::add_instance_and_snapshot_names(parser))
    {
        if (!item.has_snapshot_name())
        {
            instances.append(fmt::format("{} ", item.instance_name()));
            instance_found = true;
        }
        else
        {
            snapshots.append(fmt::format("{}.{} ", item.instance_name(), item.snapshot_name()));
            snapshot_found = true;
        }

        request.add_instance_snapshot_pairs()->CopyFrom(item);
    }

    return enforce_purged_snapshots(instances, snapshots, instance_found, snapshot_found);
}

mp::ParseCode cmd::Delete::enforce_purged_snapshots(std::string& instances,
                                                    std::string& snapshots,
                                                    bool instance_found,
                                                    bool snapshot_found)
{
    if (snapshot_found && !request.purge())
    {
        if (instance_found)
            cerr << fmt::format("{}:\n\n\tmultipass delete --purge {}\n\nYou can use a separate command to delete "
                                "instances without purging them:\n\n\tmultipass delete {}\n",
                                no_purge_base_error_msg,
                                snapshots,
                                instances);
        else
            cerr << fmt::format("{}.\n", no_purge_base_error_msg);

        return mp::ParseCode::CommandLineError;
    }

    return mp::ParseCode::Ok;
}
