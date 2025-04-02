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

#include "disable_zones.h"

#include "animated_spinner.h"
#include "common_callbacks.h"
#include "common_cli.h"
#include "multipass/cli/argparser.h"
#include "multipass/cli/client_common.h"

#include <QCommandLineOption>

namespace multipass::cmd
{
ReturnCode DisableZones::run(ArgParser* parser)
{
    if (const auto ret = parse_args(parser); ret != ParseCode::Ok)
        return parser->returnCodeFrom(ret);

    if (ask_for_confirmation)
    {
        if (!term->is_live())
            throw std::runtime_error{
                "Unable to query client for confirmation. Use '--force' to avoid prompting for confirmation."};

        if (!confirm())
            return ReturnCode::CommandFail;
    }

    AnimatedSpinner spinner{cout};
    spinner.start(format("Disabling {}", fmt::join(request.zones(), ", ")));

    const auto on_success = [&](const ZonesStateReply&) {
        spinner.stop();
        cout << fmt::format("Zone{} disabled: {}",
                            request.zones_size() == 1 ? "" : "s",
                            fmt::join(request.zones(), ", "));
        return Ok;
    };

    const auto on_failure = [this, &spinner](const grpc::Status& status) {
        spinner.stop();
        return standard_failure_handler_for(name(), cerr, status);
    };

    const auto streaming_callback = make_logging_spinner_callback<ZonesStateRequest, ZonesStateReply>(spinner, cerr);

    return dispatch(&RpcMethod::zones_state, request, on_success, on_failure, streaming_callback);
}

std::string DisableZones::name() const
{
    return "disable-zones";
}

QString DisableZones::short_help() const
{
    return QStringLiteral("Make zones unavailable");
}

QString DisableZones::description() const
{
    return QStringLiteral("Makes the given availability zones unavailable.");
}

ParseCode DisableZones::parse_args(ArgParser* parser)
{
    parser->addPositionalArgument("zone", "Name of the zones to make unavailable", "<zone> [<zone> ...]");

    QCommandLineOption forceOption{"force", "Do not ask for confirmation"};
    parser->addOptions({forceOption});

    if (const auto status = parser->commandParse(this); status != ParseCode::Ok)
        return status;

    request.set_available(false);
    request.set_verbosity_level(parser->verbosityLevel());

    for (const auto& zone_name : parser->positionalArguments())
        request.add_zones(zone_name.toStdString());

    if (request.zones().empty())
    {
        cerr << "No zones supplied" << std::endl;
        return ParseCode::CommandLineError;
    }

    ask_for_confirmation = !parser->isSet(forceOption);

    return ParseCode::Ok;
}

bool DisableZones::confirm()
{
    // joins zones by comma with an 'and' for the last one e.g. 'zone1, zone2 and zone3'
    const auto format_zones = [this] {
        if (request.zones_size() == 1)
            return request.zones(0);

        const auto last_zone = request.zones_size() - 1;
        return fmt::format("{} and {}",
                           fmt::join(request.zones().begin(), request.zones().begin() + last_zone, ", "),
                           request.zones(last_zone));
    };
    const auto message = "This operation will forcefully stop the VMs in " + format_zones() + ". Proceed? (Yes/No)";

    const PlainPrompter prompter{term};
    auto answer = prompter.prompt(message);
    while (!std::regex_match(answer, client::yes_answer) && !std::regex_match(answer, client::no_answer))
        answer = prompter.prompt("Please answer (Yes/No)");

    return std::regex_match(answer, client::yes_answer);
}
} // namespace multipass::cmd
