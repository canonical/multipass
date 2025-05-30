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

#include "block_list.h"
#include "common_cli.h"

#include <multipass/cli/argparser.h>
#include <multipass/cli/formatter.h>
#include <multipass/exceptions/cmd_exceptions.h>
#include <multipass/format.h>
#include <multipass/logging/log.h>
#include <multipass/rpc/multipass.pb.h>

namespace mp = multipass;
namespace cmd = multipass::cmd;
namespace mpl = multipass::logging;

mp::ReturnCode cmd::BlockList::run(mp::ArgParser* parser)
{
    auto ret = parse_args(parser);
    if (ret != ParseCode::Ok)
    {
        return parser->returnCodeFrom(ret);
    }

    auto on_success = [this](mp::ListBlocksReply& reply) {
        mpl::debug("client", "BlockList::on_success called");
        mpl::debug("client", "Reply has {} block devices", reply.block_devices().size());
        
        for (int i = 0; i < reply.block_devices().size(); ++i) {
            const auto& block = reply.block_devices(i);
            mpl::debug("client", "Block device {}: name={}, size={}, path={}, attached_to={}",
                       i, block.name(), block.size(), block.path(),
                       block.attached_to().empty() ? "--" : block.attached_to());
        }
        
        mpl::debug("client", "About to call formatter->format()");
        auto formatted_output = chosen_formatter->format(reply);
        mpl::debug("client", "Formatter returned: '{}'", formatted_output);
        
        cout << formatted_output;
        return ReturnCode::Ok;
    };

    auto on_failure = [this](grpc::Status& status) {
        throw mp::ValidationException{fmt::format("Failed to connect to daemon: {}", status.error_message())};
        return ReturnCode::CommandFail;
    };

    request.set_verbosity_level(parser->verbosityLevel());
    return dispatch(&RpcMethod::list_blocks, request, on_success, on_failure);
}

std::string cmd::BlockList::name() const
{
    return "block-list";
}

QString cmd::BlockList::short_help() const
{
    return QStringLiteral("List available block devices");
}

QString cmd::BlockList::description() const
{
    return QStringLiteral("List all available block devices.");
}

mp::ParseCode cmd::BlockList::parse_args(mp::ArgParser* parser)
{
    QCommandLineOption formatOption("format",
                                   "Output list in the requested format.\n"
                                   "Valid formats are: table (default), json, csv and yaml",
                                   "format",
                                   "table");

    parser->addOption(formatOption);

    auto status = parser->commandParse(this);

    if (status != ParseCode::Ok)
    {
        return status;
    }

    if (parser->positionalArguments().count() > 0)
    {
        cerr << "This command takes no arguments\n";
        return ParseCode::CommandLineError;
    }

    status = handle_format_option(parser, &chosen_formatter, cerr);

    return status;
}