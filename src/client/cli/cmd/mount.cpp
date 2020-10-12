/*
 * Copyright (C) 2017-2020 Canonical, Ltd.
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
#include "common_cli.h"
#include "animated_spinner.h"

#include <multipass/cli/argparser.h>
#include <multipass/cli/client_platform.h>
#include <multipass/format.h>
#include <multipass/logging/log.h>

#include <QDir>
#include <QFileInfo>

namespace mp = multipass;
namespace mcp = multipass::cli::platform;
namespace mpl = multipass::logging;
namespace cmd = multipass::cmd;
using RpcMethod = mp::Rpc::Stub;

namespace
{
constexpr auto category = "mount cmd";

auto convert_id_for(const QString& id_string)
{
    bool ok;

    auto id = id_string.toUInt(&ok);
    if (!ok)
        throw std::runtime_error(id_string.toStdString() + " is an invalid id");

    return id;
}
} // namespace

mp::ReturnCode cmd::Mount::run(mp::ArgParser* parser)
{
    auto ret = parse_args(parser);
    if (ret != ParseCode::Ok)
    {
        return parser->returnCodeFrom(ret);
    }

    mp::AnimatedSpinner spinner{cout};

    auto on_success = [&spinner](mp::MountReply& reply) {
        spinner.stop();
        return ReturnCode::Ok;
    };

    auto on_failure = [this, &spinner](grpc::Status& status) {
        spinner.stop();

        return standard_failure_handler_for(name(), cerr, status);
    };

    auto streaming_callback = [&spinner](mp::MountReply& reply) {
        spinner.stop();
        spinner.start(reply.mount_message());
    };

    request.set_verbosity_level(parser->verbosityLevel());

    return dispatch(&RpcMethod::mount, request, on_success, on_failure, streaming_callback);
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
                          "automatically on next boot.");
}

mp::ParseCode cmd::Mount::parse_args(mp::ArgParser* parser)
{
    parser->addPositionalArgument("source", "Path of the local directory to mount", "<source>");
    parser->addPositionalArgument("target",
                                  "Target mount points, in <name>[:<path>] format, where <name> "
                                  "is an instance name, and optional <path> is the mount point. "
                                  "If omitted, the mount point will be the same as the source's "
                                  "absolute path",
                                  "<target> [<target> ...]");

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
        return status;

    if (parser->positionalArguments().count() < 2)
    {
        cerr << "Not enough arguments given\n";
        return ParseCode::CommandLineError;
    }

    QString source_path(parser->positionalArguments().at(0));

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
#if (QT_VERSION >= QT_VERSION_CHECK(5, 15, 0))
        auto parsed_target = QString(parser->positionalArguments().at(i)).split(":", Qt::SkipEmptyParts);
#else
        auto parsed_target = QString(parser->positionalArguments().at(i)).split(":", QString::SkipEmptyParts);
#endif

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

    QRegExp map_matcher("^([0-9]+[:][0-9]+)$");

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

            try
            {
                auto host_uid = convert_id_for(parsed_map.at(0));
                auto instance_uid = convert_id_for(parsed_map.at(1));

                (*request.mutable_mount_maps()->mutable_uid_map())[host_uid] = instance_uid;
            }
            catch (const std::exception& e)
            {
                cerr << e.what() << "\n";
                return ParseCode::CommandLineError;
            }
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

            try
            {
                auto host_gid = convert_id_for(parsed_map.at(0));
                auto instance_gid = convert_id_for(parsed_map.at(1));

                (*request.mutable_mount_maps()->mutable_gid_map())[host_gid] = instance_gid;
            }
            catch (const std::exception& e)
            {
                cerr << e.what() << "\n";
                return ParseCode::CommandLineError;
            }
        }
    }

    if (!parser->isSet(uid_map) && !parser->isSet(gid_map))
    {
        mpl::log(mpl::Level::debug, category,
                 fmt::format("{}:{} {}(): adding default uid/gid mapping", __FILE__, __LINE__, __FUNCTION__));
        (*request.mutable_mount_maps()->mutable_uid_map())[mcp::getuid()] = mp::default_id;
        (*request.mutable_mount_maps()->mutable_gid_map())[mcp::getgid()] = mp::default_id;
    }

    return ParseCode::Ok;
}
