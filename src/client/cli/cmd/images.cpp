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

#include "images.h"
#include "common_cli.h"

#include <multipass/cli/argparser.h>
#include <multipass/cli/formatter.h>
#include <multipass/user_messages/deprecation_warning.h>

#include <string>

namespace multipass::cmd
{

namespace
{

constexpr std::string deprecated_name = "find";

constexpr const auto command_filter_help =
    "An optional value to search for in [<remote:>]<string> format, where "
    "<remote> can be either ‘release’ or ‘daily’. If <remote> is omitted, "
    "it will search ‘release‘ first, and if no matches are found, it will "
    "then search ‘daily‘. <string> can be a partial image hash or a "
    "release version, codename or alias.";

struct CommandOptions
{
    const QCommandLineOption list_remotes = QCommandLineOption{"remotes",
                                                               "List all available remotes."};

    const QCommandLineOption unsupported = QCommandLineOption{
        "show-unsupported",
        "Show unsupported cloud images as well"};

    const QCommandLineOption format = QCommandLineOption{
        "format",
        "Output list in the requested format.\nValid formats are: "
        "table (default), json, csv and yaml",
        "format",
        "table"};

    const QCommandLineOption force_manifest_network_download = QCommandLineOption{
        "force-update",
        "Force the image information to update from the network"};
};

std::tuple<ParseCode, ImagesRequest> parse_images_request(ArgParser& parser,
                                                          const CommandOptions& options,
                                                          std::ostream& cerr)
{
    auto request = ImagesRequest{};

    if (parser.positionalArguments().count() == 1)
    {
        auto search_string = parser.positionalArguments().first();
        auto colon_count = search_string.count(':');

        if (colon_count > 1)
        {
            cerr << "Invalid remote and search string supplied\n";
            return {ParseCode::CommandLineError, request};
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

    request.set_verbosity_level(parser.verbosityLevel());
    request.set_allow_unsupported(parser.isSet(options.unsupported));
    request.set_force_manifest_network_download(
        parser.isSet(options.force_manifest_network_download));

    return {ParseCode::Ok, request};
}

std::tuple<ParseCode, RemotesRequest> parse_remotes_request(ArgParser& parser,
                                                            const CommandOptions& options,
                                                            std::ostream& cerr)
{
    auto request = RemotesRequest{};

    if (parser.positionalArguments().count() > 0)
    {
        cerr << "Command does not expect positional arguments when using --remotes. "
                "They will be ignored.\n";
    }

    if (parser.isSet(options.force_manifest_network_download) || parser.isSet(options.unsupported))
    {
        cerr << "Options --force-manifest-network-download and --unsupported are not "
                "compatible with --remotes. They will be ignored.\n";
    }

    request.set_verbosity_level(parser.verbosityLevel());

    return {ParseCode::Ok, request};
}

} // namespace

template <typename Reply, typename RpcFunc, typename Request>
ReturnCodeVariant Images::dispatch_request(RpcFunc&& rpc_func,
                                           const Request& request,
                                           Formatter* formatter)
{
    auto on_success = [this, formatter](Reply& reply) -> ReturnCodeVariant {
        cout << formatter->format(reply);
        return ReturnCode::Ok;
    };

    auto on_failure = [this](grpc::Status& status) -> ReturnCodeVariant {
        return standard_failure_handler_for(name(), cerr, status);
    };

    return dispatch(std::forward<RpcFunc>(rpc_func), request, on_success, on_failure);
}

ReturnCodeVariant Images::run(ArgParser* parser)
{
    const auto options = CommandOptions{};

    parser->addPositionalArgument("string", command_filter_help, "[<remote:>][<string>]");
    parser->addOptions({
        options.list_remotes,
        options.unsupported,
        options.format,
        options.force_manifest_network_download,
    });

    auto status = parser->commandParse(this);
    if (status != ParseCode::Ok)
    {
        return parser->returnCodeFrom(status);
    }

    if (parser->commandName() == deprecated_name)
    {
        cerr << make_deprecation_warning(fmt::format("‘multipass {}’ command", deprecated_name),
                                         fmt::format("Use ‘multipass {}’ instead.", name()));
    }

    if (parser->positionalArguments().count() > 1)
    {
        cerr << "Wrong number of arguments\n";
        return parser->returnCodeFrom(ParseCode::CommandLineError);
    }

    Formatter* formatter = nullptr;
    status = handle_format_option(parser, &formatter, cerr);
    if (status != ParseCode::Ok)
    {
        return parser->returnCodeFrom(ParseCode::CommandLineError);
    }

    if (parser->isSet(options.list_remotes))
    {
        const auto [status, request] = parse_remotes_request(*parser, options, cerr);
        if (status != ParseCode::Ok)
        {
            return parser->returnCodeFrom(status);
        }

        return dispatch_request<RemotesReply>(&RpcMethod::remotes, request, formatter);
    }
    else
    {
        const auto [status, request] = parse_images_request(*parser, options, cerr);
        if (status != ParseCode::Ok)
        {
            return parser->returnCodeFrom(status);
        }

        return dispatch_request<ImagesReply>(&RpcMethod::images, request, formatter);
    }
}

std::string Images::name() const
{
    return "images";
}

QString Images::short_help() const
{
    return QStringLiteral("Display available images to create instances from");
}

QString Images::description() const
{
    return QStringLiteral("Lists available images matching <string> for creating instances from.\n"
                          "With no search string, lists all aliases for supported releases.");
}

std::vector<std::string> Images::aliases() const
{
    return {name(), deprecated_name};
}

} // namespace multipass::cmd
