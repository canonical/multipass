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

#include "authenticate.h"

#include "common_cli.h"

#include <multipass/cli/argparser.h>
#include <multipass/cli/prompters.h>
#include <multipass/exceptions/cli_exceptions.h>

namespace mp = multipass;
namespace cmd = multipass::cmd;

mp::ReturnCode cmd::Authenticate::run(mp::ArgParser* parser)
{
    auto ret = parse_args(parser);
    if (ret != ParseCode::Ok)
    {
        return parser->returnCodeFrom(ret);
    }

    auto on_success = [](mp::AuthenticateReply& reply) { return mp::ReturnCode::Ok; };

    auto on_failure = [this](grpc::Status& status) { return standard_failure_handler_for(name(), cerr, status); };

    request.set_verbosity_level(parser->verbosityLevel());
    return dispatch(&RpcMethod::authenticate, request, on_success, on_failure);
}

std::string cmd::Authenticate::name() const
{
    return "authenticate";
}

std::vector<std::string> cmd::Authenticate::aliases() const
{
    return {name(), "auth"};
}

QString cmd::Authenticate::short_help() const
{
    return QStringLiteral("Authenticate client");
}

QString cmd::Authenticate::description() const
{
    return QStringLiteral("Authenticate with the Multipass service.\n"
                          "A system administrator should provide you with a passphrase\n"
                          "to allow use of the Multipass service.");
}

mp::ParseCode cmd::Authenticate::parse_args(mp::ArgParser* parser)
{
    parser->addPositionalArgument("passphrase",
                                  "Passphrase to register with the Multipass service. If omitted, a prompt will be "
                                  "displayed for entering the passphrase.",
                                  "[<passphrase>]");

    auto status = parser->commandParse(this);
    if (status != ParseCode::Ok)
        return status;

    if (parser->positionalArguments().count() > 1)
    {
        cerr << "Too many arguments given\n";
        return ParseCode::CommandLineError;
    }

    if (parser->positionalArguments().empty())
    {
        try
        {
            mp::PassphrasePrompter prompter(term);
            auto passphrase = prompter.prompt();

            if (passphrase.empty())
            {
                cerr << "No passphrase given\n";
                return ParseCode::CommandLineError;
            }

            request.set_passphrase(passphrase);
        }
        catch (const mp::PromptException& e)
        {
            cerr << e.what() << std::endl;
            return ParseCode::CommandLineError;
        }
    }
    else
    {
        request.set_passphrase(parser->positionalArguments().first().toStdString());
    }

    return status;
}
