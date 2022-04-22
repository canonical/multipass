/*
 * Copyright (C) 2017-2022 Canonical, Ltd.
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

#include "exec.h"
#include "common_cli.h"

#include <multipass/cli/argparser.h>
#include <multipass/ssh/ssh_client.h>

namespace mp = multipass;
namespace cmd = multipass::cmd;

namespace
{
const QString work_dir_option_name{"working-directory"};
}

mp::ReturnCode cmd::Exec::run(mp::ArgParser* parser)
{
    auto ret = parse_args(parser);
    if (ret != ParseCode::Ok)
    {
        return parser->returnCodeFrom(ret);
    }

    std::vector<std::string> args;
    for (int i = 1; i < parser->positionalArguments().size(); ++i)
        args.push_back(parser->positionalArguments().at(i).toStdString());

    std::optional<std::string> work_dir;
    if (parser->isSet(work_dir_option_name))
        work_dir = parser->value(work_dir_option_name).toStdString();

    auto on_success = [this, &args, &work_dir](mp::SSHInfoReply& reply) {
        return exec_success(reply, work_dir, args, term);
    };

    auto on_failure = [this, parser](grpc::Status& status) {
        if (status.error_code() == grpc::StatusCode::ABORTED)
            return run_cmd_and_retry({"multipass", "start", QString::fromStdString(request.instance_name(0))}, parser,
                                     cout, cerr);
        else
            return standard_failure_handler_for(name(), cerr, status);
    };

    request.set_verbosity_level(parser->verbosityLevel());
    ReturnCode return_code;
    while ((return_code = dispatch(&RpcMethod::ssh_info, request, on_success, on_failure)) == ReturnCode::Retry)
        ;

    return return_code;
}

std::string cmd::Exec::name() const
{
    return "exec";
}

QString cmd::Exec::short_help() const
{
    return QStringLiteral("Run a command on an instance");
}

QString cmd::Exec::description() const
{
    return QStringLiteral("Run a command on an instance");
}

mp::ReturnCode cmd::Exec::exec_success(const mp::SSHInfoReply& reply, const mp::optional<std::string>& dir,
                                       const std::vector<std::string>& args, mp::Terminal* term)
{
    // TODO: mainly for testing - need a better way to test parsing
    if (reply.ssh_info().empty())
        return ReturnCode::Ok;

    const auto& ssh_info = reply.ssh_info().begin()->second;
    const auto& host = ssh_info.host();
    const auto& port = ssh_info.port();
    const auto& username = ssh_info.username();
    const auto& priv_key_blob = ssh_info.priv_key_base64();

    try
    {
        auto console_creator = [&term](auto channel) { return Console::make_console(channel, term); };
        mp::SSHClient ssh_client{host, port, username, priv_key_blob, console_creator};

        std::vector<std::vector<std::string>> all_args;
        if (dir)
            all_args = {{"cd", *dir}, {args}};
        else
            all_args = {{args}};

        return static_cast<mp::ReturnCode>(ssh_client.exec(all_args));
    }
    catch (const std::exception& e)
    {
        term->cerr() << "exec failed: " << e.what() << "\n";
        return ReturnCode::CommandFail;
    }
}

mp::ParseCode cmd::Exec::parse_args(mp::ArgParser* parser)
{
    parser->addPositionalArgument("name", "Name of instance to execute the command on", "<name>");
    parser->addPositionalArgument("command", "Command to execute on the instance", "[--] <command>");

    QCommandLineOption workDirOption({"w", work_dir_option_name}, "Change to <dir> before execution", "dir");

    parser->addOptions({workDirOption});

    auto status = parser->commandParse(this);

    if (status != ParseCode::Ok)
    {
        if (!parser->unknownOptionNames().empty() && !parser->containsArgument("--"))
        {
            bool is_alias = parser->executeAlias() != mp::nullopt;
            cerr << fmt::format("\nOptions to the {} should come after \"--\", like this:\nmultipass {} <arguments>\n",
                                is_alias ? "alias" : "inner command",
                                is_alias ? "<alias> --" : "exec <instance> -- <command>");
        }
        return status;
    }

    if (parser->positionalArguments().count() < 2)
    {
        cerr << "Wrong number of arguments\n";
        status = ParseCode::CommandLineError;
    }
    else
    {
        auto entry = request.add_instance_name();
        entry->append(parser->positionalArguments().first().toStdString());
    }

    return status;
}
