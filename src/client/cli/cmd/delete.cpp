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
#include <multipass/cli/client_common.h>
#include <multipass/cli/prompters.h>
#include <multipass/platform.h>

#include <unordered_set>

namespace mp = multipass;
namespace cmd = multipass::cmd;

mp::ReturnCodeVariant cmd::Delete::run(mp::ArgParser* parser)
{
    if (const auto ret = parse_args(parser); ret != ParseCode::Ok)
        return parser->returnCodeFrom(ret);

    if (ask_for_confirmation)
    {
        if (!term->is_live())
            throw std::runtime_error{"Unable to query client for confirmation. Use '--force' to "
                                     "delete without confirmation."};

        if (!confirm())
            return ReturnCode::CommandFail;
    }

    auto on_success = [this](mp::DeleteReply&) -> ReturnCodeVariant {
        for (const auto& item : request.instance_snapshot_pairs())
            if (item.has_snapshot_name())
                cout << fmt::format("{}.{} is deleted.\n",
                                    item.instance_name(),
                                    item.snapshot_name());

        return mp::ReturnCode::Ok;
    };

    auto on_failure = [this](grpc::Status& status) -> ReturnCodeVariant {
        return standard_failure_handler_for(name(), cerr, status);
    };

    using Client = grpc::ClientReaderWriterInterface<DeleteRequest, DeleteReply>;
    auto streaming_callback = [this](const mp::DeleteReply& reply, Client*) {
        if (!reply.log_line().empty())
            cerr << reply.log_line();

        for (const auto& instance : reply.deleted_instances())
        {
            remove_aliases_for(instance);
            cout << fmt::format("{} is deleted.\n", instance);
        }
    };

    return dispatch(&RpcMethod::delet, request, on_success, on_failure, streaming_callback);
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
    return QStringLiteral("Permanently delete instances and snapshots (in stopped instances).\n"
                          "Deleted instances and snapshots cannot be recovered after deletion.");
}

mp::ParseCode cmd::Delete::parse_args(mp::ArgParser* parser)
{
    parser->addPositionalArgument("name",
                                  "Names of instances and snapshots to delete",
                                  "<instance>[.snapshot] [<instance>[.snapshot] ...]");

    QCommandLineOption all_option(all_option_name, "Delete all instances and snapshots");
    QCommandLineOption force_option{force_option_name, "Do not ask for confirmation"};
    parser->addOptions({all_option, force_option});

    auto status = parser->commandParse(this);
    if (status != ParseCode::Ok)
        return status;

    status = check_for_name_and_all_option_conflict(parser, cerr);
    if (status != ParseCode::Ok)
        return status;

    request.set_verbosity_level(parser->verbosityLevel());
    delete_all = parser->isSet(all_option);
    ask_for_confirmation = !parser->isSet(force_option);

    const auto items = cmd::add_instance_and_snapshot_names(parser);
    std::unordered_set<std::string> instances;
    for (const auto& item : items)
        if (!item.has_snapshot_name())
            instances.insert(item.instance_name());

    // snapshots of instances being deleted go away with them
    for (const auto& item : items)
        if (!item.has_snapshot_name() || !instances.contains(item.instance_name()))
            request.add_instance_snapshot_pairs()->CopyFrom(item);

    return ParseCode::Ok;
}

bool cmd::Delete::confirm() const
{
    std::string subject = "All instances";
    if (!delete_all)
    {
        std::vector<std::string> instances, snapshots;
        for (const auto& item : request.instance_snapshot_pairs())
        {
            if (item.has_snapshot_name())
                snapshots.push_back(
                    fmt::format("{}.{}", item.instance_name(), item.snapshot_name()));
            else
                instances.push_back(item.instance_name());
        }

        // joins items by comma with an 'and' for the last one e.g. "'a', 'b' and 'c'"
        const auto format_list = [](const std::vector<std::string>& items) {
            if (items.size() == 1)
                return fmt::format("'{}'", items.front());

            return fmt::format("'{}' and '{}'",
                               fmt::join(items.begin(), items.end() - 1, "', '"),
                               items.back());
        };

        std::vector<std::string> parts;
        if (!instances.empty())
            parts.push_back(fmt::format("Instance{} {}",
                                        instances.size() == 1 ? "" : "s",
                                        format_list(instances)));
        if (!snapshots.empty())
            parts.push_back(fmt::format("{}napshot{} {}",
                                        instances.empty() ? "S" : "s",
                                        snapshots.size() == 1 ? "" : "s",
                                        format_list(snapshots)));

        subject = fmt::format("{}", fmt::join(parts, ", and "));
    }

    static constexpr auto prompt_text =
        "{} will be deleted permanently. Would you like to proceed?";

    return YesNoPrompter{term}.prompt(fmt::format(prompt_text, subject), false);
}

void cmd::Delete::remove_aliases_for(const std::string& instance)
{
    for (const auto& [removal_context, removed_alias_name] :
         aliases.remove_aliases_for_instance(instance))
    {
        try
        {
            MP_PLATFORM.remove_alias_script(removal_context + "." + removed_alias_name);

            if (!aliases.exists_alias(removed_alias_name))
                MP_PLATFORM.remove_alias_script(removed_alias_name);
        }
        catch (const std::runtime_error& e)
        {
            cerr << fmt::format("Warning: '{}' when removing alias script for {}.{}\n",
                                e.what(),
                                removal_context,
                                removed_alias_name);
        }
    }
}
