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

    // If no zones exist, just inform the user and exit successfully
    if (request.zones_size() == 0)
    {
        cout << "No zones exist" << std::endl;
        return ReturnCode::CommandFail;
    }

    // If --all was specified, we need to fetch all zone names first
    if (use_all_zones)
    {
        ZonesRequest zones_request{};
        zones_request.set_verbosity_level(parser->verbosityLevel());

        auto zones_on_success = [this](const ZonesReply& reply) {
            // Populate the request with all zone names
            for (const auto& zone : reply.zones())
            {
                request.add_zones(zone.name());
            }
            return Ok;
        };

        auto zones_on_failure = [this](const grpc::Status& status) {
            return standard_failure_handler_for(name(), cerr, status);
        };

        // First, get all zones
        if (const auto ret = dispatch(&RpcMethod::zones, zones_request, zones_on_success, zones_on_failure); ret != Ok)
            return ret;
    }

    AnimatedSpinner spinner{cout};
    const auto message =
        use_all_zones ? "Enabling all zones" : fmt::format("Enabling {}", fmt::join(request.zones(), ", "));
    spinner.start(message);

    const auto on_success = [&](const ZonesStateReply&) {
        spinner.stop();
        const auto output_message = use_all_zones ? "All zones enabled"
                                                  : fmt::format("Zone{} enabled: {}",
                                                                request.zones_size() == 1 ? "" : "s",
                                                                fmt::join(request.zones(), ", "));
        cout << output_message << std::endl;
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

    QCommandLineOption all_option(all_option_name, "Enable all zones");
    parser->addOption(all_option);

    if (const auto status = parser->commandParse(this); status != ParseCode::Ok)
        return status;

    if (const auto status = check_for_name_and_all_option_conflict(parser, cerr); status != ParseCode::Ok)
        return status;

    request.set_available(true);
    request.set_verbosity_level(parser->verbosityLevel());

    // Store whether --all was used for later processing in run()
    use_all_zones = parser->isSet(all_option_name);

    if (!use_all_zones)
    {
        for (const auto& zone_name : parser->positionalArguments())
            request.add_zones(zone_name.toStdString());
    }

    return ParseCode::Ok;
}
} // namespace multipass::cmd
