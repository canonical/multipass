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

#include "mount.h"
#include "exec.h"

#include <multipass/cli/argparser.h>

#include <QDir>
#include <QFileInfo>

namespace mp = multipass;
namespace cmd = multipass::cmd;
using RpcMethod = mp::Rpc::Stub;

mp::ReturnCode cmd::Mount::run(mp::ArgParser* parser)
{
    auto ret = parse_args(parser);
    if (ret != ParseCode::Ok)
    {
        return parser->returnCodeFrom(ret);
    }

    auto on_success = [](mp::MountReply& reply) {
        return ReturnCode::Ok;
    };

    auto on_failure = [this](grpc::Status& status) {
        cerr << "mount failed: " << status.error_message() << "\n";

        if (!status.error_details().empty())
        {
            mp::MountError mount_error;
            mount_error.ParseFromString(status.error_details());

            if (mount_error.error_code() == mp::MountError::SSHFS_MISSING)
            {
                cerr << "The sshfs package is missing in \"" << mount_error.instance_name() << "\". Installing...\n";

                if (install_sshfs(mount_error.instance_name()) == mp::ReturnCode::Ok)
                    cerr << "\n***Please re-run the mount command.\n";
            }
        }
        return ReturnCode::CommandFail;
    };

    return dispatch(&RpcMethod::mount, request, on_success, on_failure);
}

std::string cmd::Mount::name() const { return "mount"; }

QString cmd::Mount::short_help() const
{
    return QStringLiteral("Mount a local directory in the instance");
}

QString cmd::Mount::description() const
{
    return QStringLiteral("Mount a local directory inside the instance. If the instance is\n"
                          "not currently running, the directory will be mounted\n"
                          "automatically on instance boot.");
}

mp::ParseCode cmd::Mount::parse_args(mp::ArgParser* parser)
{
    parser->addPositionalArgument("source", "Path of the directory to mount, in [<name>:]<path> format, "
                                            "where <name> can be either an instance name, or the string "
                                            "\"remote\", meaning that the directory being mounted resides "
                                            "on the host running multipassd", "<source>");
    parser->addPositionalArgument("target", "Target mount points, in <name>[:<path>] format, where <name> "
                                            "is an instance name, and optional <path> is the mount point. "
                                            "If omitted, the mount point will be the same as the source's "
                                            "<path>", "<target> [<target> ...]");

    QCommandLineOption gid_map({"g", "gid-map"}, "A mapping of group IDs for use in the mount. "
                                                 "File and folder ownership will be mapped from "
                                                 "<host> to <instance> inside the instance. Can be "
                                                 "used multiple times.", "host>:<instance");
    QCommandLineOption uid_map({"u", "uid-map"}, "A mapping of user IDs for use in the mount. "
                                                 "File and folder ownership will be mapped from "
                                                 "<host> to <instance> inside the instance. Can be "
                                                 "used multiple times.", "host>:<instance");
    parser->addOptions({gid_map, uid_map});

    auto status = parser->commandParse(this);

    if (status != ParseCode::Ok)
    {
        return status;
    }

    if (parser->positionalArguments().count() < 2)
    {
        cerr << "Not enough arguments given\n";
        status = ParseCode::CommandLineError;
    }
    else
    {
        QString source(parser->positionalArguments().at(0));

        QString source_path;
        auto colon_count = source.count(':');

        if (colon_count > 2)
        {
            cerr << "Invalid source path given\n";
            return ParseCode::CommandLineError;
        }
        else if (colon_count == 2)
        {
            // Check to see if we are Windows
            if (source.section(':', 1, 1).size() != 1)
            {
                cerr << "Invalid source path given" << std::endl;
                return ParseCode::CommandLineError;
            }

            // TODO: If [<name>:] is specified, make sure it's "remote".
            // Change this when we support instance to instance mounts.
            if (!source.startsWith("remote"))
            {
                cerr << "Source path needs to start with \"remote:\"\n";
                return ParseCode::CommandLineError;
            }

            source_path = source.section(':', 1);
        }
        else if (colon_count == 1)
        {
            // Check to see if we are not Windows
            if (source.section(':', 0, 0).size() != 1)
            {
                // TODO: If [<name>:] is specified, make sure it's "remote".
                // Change this when we support instance to instance mounts.
                if (!source.startsWith("remote"))
                {
                    cerr << "Source path needs to start with \"remote:\"" << std::endl;
                    return ParseCode::CommandLineError;
                }

                source_path = source.section(':', 1);
            }
            else
            {
                source_path = source;
            }
        }
        else
        {
            source_path = source;
        }

        // Validate source directory of client side mounts
        QFileInfo source_dir(source_path);
        if (!source_dir.exists())
        {
            cerr << "Source path \"" << source_path.toStdString() << "\" does not exist\n";
            return ParseCode::CommandLineError;
        }

        if (!source_dir.isDir())
        {
            cerr << "Source path \"" << source_path.toStdString() << "\" is not a directory\n";
            return ParseCode::CommandLineError;
        }

        if (!source_dir.isReadable())
        {
            cerr << "Source path \"" << source_path.toStdString() << "\" is not readable\n";
            return ParseCode::CommandLineError;
        }

        source_path = QDir(source_path).absolutePath();
        request.set_source_path(source_path.toStdString());

        for (auto i = 1; i < parser->positionalArguments().count(); ++i)
        {
            auto parsed_target = QString(parser->positionalArguments().at(i)).split(":");

            auto entry = request.add_target_paths();
            entry->set_instance_name(parsed_target.at(0).toStdString());

            if (parsed_target.count() == 1)
            {
                entry->set_target_path(source_path.toStdString());
            }
            else
            {
                entry->set_target_path(parsed_target.at(1).toStdString());
            }
        }
    }

    QRegExp map_matcher("^([0-9]{1,5}[:][0-9]{1,5})$");

    if (parser->isSet(uid_map))
    {
        auto uid_maps = parser->values(uid_map);

        for (const auto& map : uid_maps)
        {
            if (!map_matcher.exactMatch(map))
            {
                cerr << "Invalid UID map given: " << map.toStdString() << "\n";
                return ParseCode::CommandLineError;
            }

            auto parsed_map = map.split(":");

            auto entry = request.add_uid_maps();
            entry->set_host_uid(parsed_map.at(0).toInt());
            entry->set_instance_uid(parsed_map.at(1).toInt());
        }
    }

    if (parser->isSet(gid_map))
    {
        auto gid_maps = parser->values(gid_map);

        for (const auto& map : gid_maps)
        {
            if (!map_matcher.exactMatch(map))
            {
                cerr << "Invalid GID map given: " << map.toStdString() << "\n";
                return ParseCode::CommandLineError;
            }

            auto parsed_map = map.split(":");

            auto entry = request.add_gid_maps();
            entry->set_host_gid(parsed_map.at(0).toInt());
            entry->set_instance_gid(parsed_map.at(1).toInt());
        }
    }

    return status;
}

mp::ReturnCode cmd::Mount::install_sshfs(const std::string& instance_name)
{
    SSHInfoRequest request;
    request.set_instance_name(instance_name);

    std::vector<std::string> args{"sudo", "bash", "-c", "apt update && apt install -y sshfs"};

    auto on_success = [this, &args](mp::SSHInfoReply& reply) { return cmd::Exec::exec_success(reply, args, cerr); };

    auto on_failure = [this](grpc::Status& status) {
        cerr << "exec failed: " << status.error_message() << "\n";
        return ReturnCode::CommandFail;
    };

    return dispatch(&RpcMethod::ssh_info, request, on_success, on_failure);
}
