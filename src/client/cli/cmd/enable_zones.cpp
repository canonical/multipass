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

#include "enable_zones.h"

#include "animated_spinner.h"
#include "common_callbacks.h"
#include "common_cli.h"
#include "multipass/cli/argparser.h"
#include "multipass/cli/client_common.h"

#include <QCommandLineOption>

namespace multipass::cmd
{
ReturnCode EnableZones::run(ArgParser* parser)
{
    if (const auto ret = parse_args(parser); ret != ParseCode::Ok)
        return parser->returnCodeFrom(ret);

    AnimatedSpinner spinner{cout};
    spinner.start(format("Enabling {}", fmt::join(request.zones(), ", ")));

    const auto on_success = [&](const ZonesStateReply&) {
        spinner.stop();
        cout << fmt::format("Zone{} enabled: {}",
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

std::string EnableZones::name() const
{
    return "enable-zones";
}

QString EnableZones::short_help() const
{
    return QStringLiteral("Make zones available");
}

QString EnableZones::description() const
{
    return QStringLiteral("Makes the given availability zones available.");
}

ParseCode EnableZones::parse_args(ArgParser* parser)
{
    parser->addPositionalArgument("zone", "Name of the zones to make available", "<zone> [<zone> ...]");

    if (const auto status = parser->commandParse(this); status != ParseCode::Ok)
        return status;

    request.set_available(true);
    request.set_verbosity_level(parser->verbosityLevel());

    for (const auto& zone_name : parser->positionalArguments())
        request.add_zones(zone_name.toStdString());

    if (request.zones().empty())
    {
        cerr << "No zones supplied" << std::endl;
        return ParseCode::CommandLineError;
    }

    return ParseCode::Ok;
}
} // namespace multipass::cmd
