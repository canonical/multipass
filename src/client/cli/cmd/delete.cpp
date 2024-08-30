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
#include <multipass/cli/prompters.h>
#include <multipass/platform.h>

namespace mp = multipass;
namespace cmd = multipass::cmd;

namespace
{
constexpr auto snapshot_purge_notice_msg = "Snapshots can only be purged (after deletion, they cannot be recovered)";
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

    auto on_failure = [this](grpc::Status& status) {
        // grpc::StatusCode::INVALID_ARGUMENT matches mp::VMStateInvalidException
        return status.error_code() == grpc::StatusCode::INVALID_ARGUMENT
                   ? standard_failure_handler_for(name(), cerr, status, "Use --purge to forcefully delete it.")
                   : standard_failure_handler_for(name(), cerr, status);
    };

    using Client = grpc::ClientReaderWriterInterface<DeleteRequest, DeleteReply>;
    auto streaming_callback = [this](const mp::DeleteReply& reply, Client* client) {
        if (!reply.log_line().empty())
            cerr << reply.log_line();

        // TODO refactor with bridging and restore prompts
        if (reply.confirm_snapshot_purging())
        {
            DeleteRequest client_response;

            if (term->is_live())
                client_response.set_purge_snapshots(confirm_snapshot_purge());
            else
                throw std::runtime_error{generate_snapshot_purge_msg()};

            client->Write(client_response);
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
    request.set_verbosity_level(parser->verbosityLevel());

    status = parse_instances_snapshots(parser);

    return status;
}

mp::ParseCode cmd::Delete::parse_instances_snapshots(mp::ArgParser* parser)
{
    for (const auto& item : cmd::add_instance_and_snapshot_names(parser))
    {
        if (!item.has_snapshot_name())
            instance_args.append(fmt::format("{} ", item.instance_name()));
        else
            snapshot_args.append(fmt::format("{}.{} ", item.instance_name(), item.snapshot_name()));

        request.add_instance_snapshot_pairs()->CopyFrom(item);
    }

    return mp::ParseCode::Ok;
}

// TODO refactor with bridging and restore prompts
bool multipass::cmd::Delete::confirm_snapshot_purge() const
{
    static constexpr auto prompt_text = "{}. Are you sure you want to continue? (Yes/no)";
    static constexpr auto invalid_input = "Please answer Yes/no";
    mp::PlainPrompter prompter{term};

    auto answer = prompter.prompt(fmt::format(prompt_text, snapshot_purge_notice_msg));
    while (!answer.empty() && !std::regex_match(answer, mp::client::yes_answer) &&
           !std::regex_match(answer, mp::client::no_answer))
        answer = prompter.prompt(invalid_input);

    return std::regex_match(answer, mp::client::yes_answer);
}

std::string multipass::cmd::Delete::generate_snapshot_purge_msg() const
{
    const auto no_purge_base_error_msg = fmt::format("{}. Unable to query client for confirmation. Please use the "
                                                     "`--purge` flag if that is what you want",
                                                     snapshot_purge_notice_msg);

    if (!instance_args.empty())
        return fmt::format("{}:\n\n\tmultipass delete --purge {}\n\nYou can use a separate command to delete "
                           "instances without purging them:\n\n\tmultipass delete {}\n",
                           no_purge_base_error_msg,
                           snapshot_args,
                           instance_args);
    else
        return fmt::format("{}.\n", no_purge_base_error_msg);
}
