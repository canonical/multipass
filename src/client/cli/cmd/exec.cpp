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

#include "exec.h"
#include "common_cli.h"

#include <multipass/cli/argparser.h>
#include <multipass/ssh/ssh_client.h>

namespace mp = multipass;
namespace cmd = multipass::cmd;

namespace
{
const QString work_dir_option_name{"working-directory"};
const QString no_dir_mapping_option{"no-map-working-directory"};

auto is_dir_mounted(const QStringList& split_current_dir, const QStringList& split_source_dir)
{
    auto source_dir_size = split_source_dir.size();

    if (split_current_dir.size() < source_dir_size)
        return false;

    for (int i = 0; i < source_dir_size; ++i)
        if (split_current_dir[i] != split_source_dir[i])
            return false;

    return true;
}
} // namespace

mp::ReturnCode cmd::Exec::run(mp::ArgParser* parser)
{
    auto ret = parse_args(parser);
    if (ret != ParseCode::Ok)
    {
        return parser->returnCodeFrom(ret);
    }

    auto instance_name = ssh_info_request.instance_name(0);

    std::vector<std::string> args;
    for (int i = 1; i < parser->positionalArguments().size(); ++i)
        args.push_back(parser->positionalArguments().at(i).toStdString());

    std::optional<std::string> work_dir;
    if (parser->isSet(work_dir_option_name))
    {
        // If the user asked for a working directory, prepend the appropriate `cd`.
        work_dir = parser->value(work_dir_option_name).toStdString();
    }
    else
    {
        // Decide whether the working directory must be mapped. There are two cases to consider:
        // 1. when executing an alias, see if the working directory is set to "map";
        // 2. when not executing an alias, see if the user did not specify the no-mapping argument.
        // If one of these two things is true, then prepend the appropriate `cd` to the command to be ran.
        if ((parser->executeAlias() && parser->executeAlias()->working_directory == "map") ||
            (!parser->executeAlias() && !parser->isSet(no_dir_mapping_option)))
        {
            // The host directory on which the user is executing the command.
            QString clean_exec_dir = QDir::cleanPath(QDir::current().canonicalPath());
            QStringList split_exec_dir = clean_exec_dir.split('/');

            auto on_info_success = [&work_dir, &split_exec_dir](mp::InfoReply& reply) {
                for (const auto& mount : reply.details(0).mount_info().mount_paths())
                {
                    auto source_dir = QDir(QString::fromStdString(mount.source_path()));
                    auto clean_source_dir = QDir::cleanPath(source_dir.absolutePath());
                    QStringList split_source_dir = clean_source_dir.split('/');

                    // If the directory is mounted, we need to `cd` to it in the instance before executing the command.
                    if (is_dir_mounted(split_exec_dir, split_source_dir))
                    {
                        for (int i = 0; i < split_source_dir.size(); ++i)
                            split_exec_dir.removeFirst();
                        work_dir = mount.target_path() + '/' + split_exec_dir.join('/').toStdString();
                    }
                }

                return ReturnCode::Ok;
            };

            auto on_info_failure = [this](grpc::Status& status) {
                return standard_failure_handler_for(name(), cerr, status);
            };

            info_request.set_verbosity_level(parser->verbosityLevel());

            info_request.add_instance_snapshot_pairs()->set_instance_name(instance_name);
            info_request.set_no_runtime_information(true);

            dispatch(&RpcMethod::info, info_request, on_info_success, on_info_failure);
            // TODO: what to do with the returned value?
        }
    }

    auto on_success = [this, &args, &work_dir](mp::SSHInfoReply& reply) {
        return exec_success(reply, work_dir, args, term);
    };

    auto on_failure = [this, &instance_name, parser](grpc::Status& status) {
        if (status.error_code() == grpc::StatusCode::ABORTED)
            return run_cmd_and_retry({"multipass", "start", QString::fromStdString(instance_name)}, parser, cout, cerr);
        else
            return standard_failure_handler_for(name(), cerr, status);
    };

    ssh_info_request.set_verbosity_level(parser->verbosityLevel());
    ReturnCode ssh_return_code;
    while ((ssh_return_code = dispatch(&RpcMethod::ssh_info, ssh_info_request, on_success, on_failure)) ==
           ReturnCode::Retry)
        ;

    return ssh_return_code;
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

mp::ReturnCode cmd::Exec::exec_success(const mp::SSHInfoReply& reply, const std::optional<std::string>& dir,
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
        auto console_creator = [&term](auto channel) { return term->make_console(channel); };
        mp::SSHClient ssh_client{host, port, username, priv_key_blob, console_creator};

        std::vector<std::vector<std::string>> all_args;
        if (dir)
        {
            if (args[0] == "sudo")
            {
                // If we are running through 'sudo' and need to change directory, it might happen that the default user
                // does not have access to the folder and thus the cd command will fail. Additionally, `cd` cannot be
                // ran with sudo, what forces us to run everything through `sh`.
                auto sh_args = fmt::format("cd {} && {}", *dir, fmt::join(args, " "));
                all_args = {{"sudo", "sh", "-c", sh_args}};
            }
            else
                all_args = {{"cd", *dir}, {args}};
        }
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

    QCommandLineOption workDirOption({"d", work_dir_option_name}, "Change to <dir> before execution", "dir");
    QCommandLineOption noDirMappingOption({"n", no_dir_mapping_option},
                                          "Do not map the host execution path to a mounted path");

    parser->addOptions({workDirOption});
    parser->addOptions({noDirMappingOption});

    auto status = parser->commandParse(this);

    if (status != ParseCode::Ok)
    {
        if (!parser->unknownOptionNames().empty() && !parser->containsArgument("--"))
        {
            bool is_alias = parser->executeAlias() != std::nullopt;
            cerr << fmt::format("\nOptions to the {} should come after \"--\", like this:\nmultipass {} <arguments>\n",
                                is_alias ? "alias" : "inner command",
                                is_alias ? "<alias> --" : "exec <instance> -- <command>");
        }
        return status;
    }

    if (parser->isSet(work_dir_option_name) && parser->isSet(no_dir_mapping_option))
    {
        cerr << fmt::format("Options --{} and --{} clash\n", work_dir_option_name, no_dir_mapping_option);
        status = ParseCode::CommandLineError;
    }
    else if (parser->positionalArguments().count() < 2)
    {
        cerr << "Wrong number of arguments\n";
        status = ParseCode::CommandLineError;
    }
    else
    {
        auto entry = ssh_info_request.add_instance_name();
        entry->append(parser->positionalArguments().first().toStdString());
    }

    return status;
}
