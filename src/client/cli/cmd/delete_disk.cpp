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

#include "delete_disk.h"
#include "common_cli.h"

#include <multipass/cli/argparser.h>
#include <multipass/cli/prompters.h>

namespace mp = multipass;
namespace cmd = multipass::cmd;

mp::ReturnCode cmd::DeleteDisk::run(mp::ArgParser* parser)
{
    auto ret = parse_args(parser);
    if (ret != ParseCode::Ok)
    {
        return parser->returnCodeFrom(ret);
    }

    // Ask for confirmation when deleting all disks
    if (delete_all && !confirm_disk_delete())
    {
        return ReturnCode::Ok;
    }

    auto on_success = [this](mp::DeleteDiskReply& reply) {
        if (delete_all)
        {
            cout << "Successfully deleted all disks\n";
        }
        else
        {
            cout << "Successfully deleted disk: " << request.name() << "\n";
        }
        return ReturnCode::Ok;
    };

    auto on_failure = [this](grpc::Status& status) {
        return standard_failure_handler_for(name(), cerr, status);
    };

    request.set_verbosity_level(parser->verbosityLevel());
    return dispatch(&RpcMethod::delete_disk, request, on_success, on_failure);
}

std::string cmd::DeleteDisk::name() const
{
    return "delete-disk";
}

std::vector<std::string> cmd::DeleteDisk::aliases() const
{
    return {"delete-disk"};
}

QString cmd::DeleteDisk::short_help() const
{
    return QStringLiteral("Delete a disk or all disks");
}

QString cmd::DeleteDisk::description() const
{
    return QStringLiteral(
        "Delete a disk or all disks. The disk must not be attached to any instance.\n\n"
        "If --all is used, all unattached disks will be deleted after confirmation.");
}

mp::ParseCode cmd::DeleteDisk::parse_args(mp::ArgParser* parser)
{
    QCommandLineOption all_option(all_option_name, "Delete all disks");
    parser->addOptions({all_option});

    auto status = parser->commandParse(this);

    if (status != ParseCode::Ok)
    {
        return status;
    }

    status = check_for_name_and_all_option_conflict(parser, cerr);
    if (status != ParseCode::Ok)
        return status;

    delete_all = parser->isSet(all_option);

    auto positionalArgs = parser->positionalArguments();
    if (delete_all)
    {
        if (positionalArgs.count() > 0)
        {
            cerr << "Cannot specify disk names when using --all\n";
            return ParseCode::CommandLineError;
        }
    }
    else
    {
        if (positionalArgs.count() != 1)
        {
            cerr << "This command requires exactly one argument: the disk name\n";
            cerr << "Usage: multipass disk-delete <disk-name>\n";
            return ParseCode::CommandLineError;
        }
        request.set_name(positionalArgs[0].toStdString());
    }

    return status;
}

bool cmd::DeleteDisk::confirm_disk_delete() const
{
    static constexpr auto prompt_text =
        "This will delete all unattached disks. Are you sure you want to continue? (Yes/no)";
    static constexpr auto invalid_input = "Please answer Yes/no";
    mp::PlainPrompter prompter{term};

    auto answer = prompter.prompt(prompt_text);
    while (!answer.empty() && !std::regex_match(answer, mp::client::yes_answer) &&
           !std::regex_match(answer, mp::client::no_answer))
        answer = prompter.prompt(invalid_input);

    return std::regex_match(answer, mp::client::yes_answer);
}

std::string cmd::DeleteDisk::generate_disk_delete_msg() const
{
    return "Unable to query client for confirmation. Please use individual commands to delete "
           "disks:\n\n"
           "multipass disk-delete <disk-name>\n";
}
