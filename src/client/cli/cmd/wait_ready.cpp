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

#include <multipass/timer.h>
#include <multipass/constants.h>
#include <multipass/exceptions/cmd_exceptions.h>
#include <multipass/cli/argparser.h>
#include <multipass/cli/formatter.h>

#include <chrono>
#include <thread>

namespace mp = multipass;
namespace cmd = multipass::cmd;

mp::ReturnCode cmd::WaitReady::run(mp::ArgParser* parser)
{
    auto ret = parse_args(parser);
    if (ret != ParseCode::Ok)
    {
        return parser->returnCodeFrom(ret);
    }
    
    mp::AnimatedSpinner spinner(cout);
    spinner.start("Waiting for Multipass daemon to be ready");

    std::unique_ptr<mp::utils::Timer> timer;

    // If the user has specified a timeout, we will create a timer
    if (parser->isSet("timeout")){
        timer = cmd::make_timer(parser->value("timeout").toInt(),
                                &spinner,
                                cerr,
                                "Timed out waiting for Multipass daemon to be ready.");
        timer->start();
    }
    
    auto on_success = [this, &spinner, &timer](WaitReadyReply& reply) {
        if (timer)
            timer->stop();
        spinner.stop();
        return ReturnCode::Ok;
    };

    auto on_failure = [this, &spinner, &timer](grpc::Status& status) {

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
        spinner.stop();
        
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
        "Wait for the Multipass daemon to be ready. This command will block until the\n"
        "daemon has initialized, fetched up-to-date image information, and is ready to\n"
        "accept requests. Its main use is to prevent failures caused by incomplete\n"
        "initialization in batch operations. An optional timeout aborts the command if\n"
        "reached.");
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
