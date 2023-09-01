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

#include "find.h"
#include "common_cli.h"

#include <multipass/cli/argparser.h>
#include <multipass/cli/formatter.h>

namespace mp = multipass;
namespace cmd = multipass::cmd;

mp::ReturnCode cmd::Find::run(mp::ArgParser* parser)
{
    auto ret = parse_args(parser);
    if (ret != ParseCode::Ok)
    {
        return parser->returnCodeFrom(ret);
    }

    auto on_success = [this](FindReply& reply) {
        cout << chosen_formatter->format(reply);

        return ReturnCode::Ok;
    };

    auto on_failure = [this](grpc::Status& status) { return standard_failure_handler_for(name(), cerr, status); };

    request.set_verbosity_level(parser->verbosityLevel());
    return dispatch(&RpcMethod::find, request, on_success, on_failure);
}

std::string cmd::Find::name() const
{
    return "find";
}

QString cmd::Find::short_help() const
{
    return QStringLiteral("Display available images to create instances from");
}

QString cmd::Find::description() const
{
    return QStringLiteral("Lists available images matching <string> for creating instances from.\n"
                          "With no search string, lists all aliases for supported Ubuntu releases.");
}

mp::ParseCode cmd::Find::parse_args(mp::ArgParser* parser)
{
    parser->addPositionalArgument("string",
                                  "An optional value to search for in [<remote:>]<string> format, where "
                                  "<remote> can be either ‘release’ or ‘daily’. If <remote> is omitted, "
                                  "it will search ‘release‘ first, and if no matches are found, it will "
                                  "then search ‘daily‘. <string> can be a partial image hash or an "
                                  "Ubuntu release version, codename or alias.",
                                  "[<remote:>][<string>]");
    QCommandLineOption unsupportedOption("show-unsupported", "Show unsupported cloud images as well");
    QCommandLineOption imagesOnlyOption("only-images", "Show only images");
    QCommandLineOption blueprintsOnlyOption("only-blueprints", "Show only blueprints");
    QCommandLineOption formatOption(
        "format", "Output list in the requested format.\nValid formats are: table (default), json, csv and yaml",
        "format", "table");
    const QCommandLineOption force_manifest_network_download("force-update",
                                                             "Force the image information to update from the network");
    parser->addOptions(
        {unsupportedOption, imagesOnlyOption, blueprintsOnlyOption, formatOption, force_manifest_network_download});

    auto status = parser->commandParse(this);

    if (status != ParseCode::Ok)
    {
        return status;
    }

    if (parser->isSet(imagesOnlyOption) && parser->isSet(blueprintsOnlyOption))
    {
        cerr << "Specify one of \"--only-images\", \"--only-blueprints\" or omit to fetch both\n";
        return ParseCode::CommandLineError;
    }

    if (parser->isSet(force_manifest_network_download) && parser->isSet(blueprintsOnlyOption))
    {
        cerr << "Force updating blueprints is not currently supported\n";
        return ParseCode::CommandLineError;
    }

    request.set_show_images(!parser->isSet(blueprintsOnlyOption));
    request.set_show_blueprints(!parser->isSet(imagesOnlyOption));

    if (parser->positionalArguments().count() > 1)
    {
        cerr << "Wrong number of arguments\n";
        return ParseCode::CommandLineError;
    }
    else if (parser->positionalArguments().count() == 1)
    {
        auto search_string = parser->positionalArguments().first();
        auto colon_count = search_string.count(':');

        if (colon_count > 1)
        {
            cerr << "Invalid remote and search string supplied\n";
            return ParseCode::CommandLineError;
        }
        else if (colon_count == 1)
        {
            request.set_remote_name(search_string.section(':', 0, 0).toStdString());
            request.set_search_string(search_string.section(':', 1).toStdString());
        }
        else
        {
            request.set_search_string(search_string.toStdString());
        }
    }

    request.set_allow_unsupported(parser->isSet(unsupportedOption));
    request.set_force_manifest_network_download(parser->isSet(force_manifest_network_download));

    status = handle_format_option(parser, &chosen_formatter, cerr);

    return status;
}
