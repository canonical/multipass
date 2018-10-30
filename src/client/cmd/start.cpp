/*
 * Copyright (C) 2017-2018 Canonical, Ltd.
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

#include "start.h"
#include "common_cli.h"
#include "exec.h"

#include "animated_spinner.h"

#include <multipass/cli/argparser.h>

namespace mp = multipass;
namespace cmd = multipass::cmd;
using RpcMethod = mp::Rpc::Stub;

mp::ReturnCode cmd::Start::run(mp::ArgParser* parser)
{
    auto ret = parse_args(parser);
    if (ret != ParseCode::Ok)
    {
        return parser->returnCodeFrom(ret);
    }

    auto on_success = [](mp::StartReply& reply) {
        return ReturnCode::Ok;
    };

    AnimatedSpinner spinner{cout};
    auto on_failure = [this, &spinner](grpc::Status& status) {
        spinner.stop();
        cerr << "start failed: " << status.error_message() << "\n";
        if (!status.error_details().empty())
        {
            if (status.error_code() == grpc::StatusCode::ABORTED)
            {
                mp::StartError start_error;
                start_error.ParseFromString(status.error_details());

                if (start_error.error_code() == mp::StartError::INSTANCE_DELETED)
                {
                    cerr << "Use 'recover' to recover the deleted instance or 'purge' to permanently delete the "
                            "instance.\n";
                }
            }
            else
            {
                mp::MountError mount_error;
                mount_error.ParseFromString(status.error_details());

                if (mount_error.error_code() == mp::MountError::SSHFS_MISSING)
                {
                    cerr << "The sshfs package is missing in \"" << mount_error.instance_name()
                         << "\". Installing...\n";

                    if (install_sshfs(mount_error.instance_name()) == mp::ReturnCode::Ok)
                        cerr << "\n***Please restart the instance to enable mount(s).\n";
                }
            }
        }

        return return_code_for(status.error_code());
    };

    std::string message{"Starting "};
    if (request.instance_names().instance_name().empty())
        message.append("all instances");
    else if (request.instance_names().instance_name().size() > 1)
        message.append("requested instances");
    else
        message.append(request.instance_names().instance_name().Get(0));
    spinner.start(message);

    request.set_verbosity_level(parser->verbosityLevel());
    return dispatch(&RpcMethod::start, request, on_success, on_failure);
}

std::string cmd::Start::name() const { return "start"; }

QString cmd::Start::short_help() const
{
    return QStringLiteral("Start instances");
}

QString cmd::Start::description() const
{
    return QStringLiteral("Start the named instances. Exits with return code 0\n"
                          "when the instances start, or with an error code if\n"
                          "any fail to start.");
}

mp::ParseCode cmd::Start::parse_args(mp::ArgParser* parser)
{
    parser->addPositionalArgument("name", "Names of instances to start", "<name> [<name> ...]");

    QCommandLineOption all_option(all_option_name, "Start all instances");
    parser->addOption(all_option);

    auto status = parser->commandParse(this);
    if (status != ParseCode::Ok)
        return status;

    auto parse_code = handle_all_option(parser);
    if (parse_code != ParseCode::Ok)
        return parse_code;

    request.mutable_instance_names()->CopyFrom(add_instance_names(parser));

    return status;
}

mp::ReturnCode cmd::Start::install_sshfs(const std::string& instance_name)
{
    SSHInfoRequest request;
    auto entry = request.add_instance_name();
    entry->append(instance_name);

    std::vector<std::string> args{"sudo", "bash", "-c", "apt update && apt install -y sshfs"};

    auto on_success = [this, &args](mp::SSHInfoReply& reply) { return cmd::Exec::exec_success(reply, args, cerr); };

    auto on_failure = [this](grpc::Status& status) {
        cerr << "exec failed: " << status.error_message() << "\n";
        return return_code_for(status.error_code());
    };

    return dispatch(&RpcMethod::ssh_info, request, on_success, on_failure);
}
