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

#include "wait_ready.h"
#include "common_cli.h"

#include "animated_spinner.h"
#include <multipass/exceptions/cmd_exceptions.h>
#include <multipass/cli/argparser.h>
#include <multipass/cli/formatter.h>
#include <multipass/timer.h>

namespace mp = multipass;
namespace cmd = multipass::cmd;
namespace mpl = multipass::logging;

mp::ReturnCode cmd::WaitReady::run(mp::ArgParser* parser)
{
    auto ret = parse_args(parser);
    if (ret != ParseCode::Ok)
    {
        return parser->returnCodeFrom(ret);
    }

    if (!spinner){
        spinner = std::make_unique<multipass::AnimatedSpinner>(cout);
        spinner->start("Waiting for Multipass daemon to be ready...");
    }

    // If the user has specified a timeout, we will create a timer
    if (parser->isSet("timeout")){
        timer = cmd::make_timer(parser->value("timeout").toInt(),
                                spinner.get(),
                                cerr,
                                "Timed out waiting for Multipass daemon to be ready.");
        timer->start();
    }
    
    auto on_success = [this](WaitReadyReply& reply) {
        if (timer)
            timer->stop();
        spinner->stop();
        return ReturnCode::Ok;
    };

    auto on_failure = [this](grpc::Status& status) {

        if (status.error_code() == grpc::StatusCode::NOT_FOUND && 
            status.error_message() == "cannot connect to the multipass socket")
        {
            // This is the expected state for when the daemon is not yet ready
            // Sleep for a short duration and signal to retry
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            return ReturnCode::Retry;
        }

        if (timer)
            timer->stop();
        spinner->stop();
        
        // For any other error, we will handle it as a standard failure
        return standard_failure_handler_for(name(), cerr, status);
    };

    request.set_verbosity_level(parser->verbosityLevel());
    
    ReturnCode return_code;

    while ((return_code = dispatch(&RpcMethod::wait_ready, request, on_success, on_failure)) == 
            ReturnCode::Retry)
        ;

    return return_code;
}

std::string cmd::WaitReady::name() const
{
    return "wait-ready";
}

QString cmd::WaitReady::short_help() const
{
    return QStringLiteral("Wait for the Multipass daemon to be ready");
}

QString cmd::WaitReady::description() const
{
    return QStringLiteral(
        "Wait for the Multipass daemon to be ready.\n"
        "This command will block until the daemon is ready to accept requests.\n"
        "It can be used to ensure that the daemon is running before executing "
        "other commands.\n"
        "If a timeout is specified, it will wait for that duration before "
        "exiting with an error if the daemon is not ready.");
}

mp::ParseCode cmd::WaitReady::parse_args(mp::ArgParser* parser)
{
    mp::cmd::add_timeout(parser);

    auto status = parser->commandParse(this);

    // Check if the command was parsed successfully
    if (status != ParseCode::Ok)
    {
        return status;
    }

    try
    {
        mp::cmd::parse_timeout(parser);
    }
    catch (const mp::ValidationException& e)
    {
        cerr << "error: " << e.what() << "\n";
        return ParseCode::CommandLineError;
    }

    return status;
}
