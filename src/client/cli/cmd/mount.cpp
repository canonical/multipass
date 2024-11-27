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

#include "mount.h"

#include "animated_spinner.h"
#include "common_callbacks.h"
#include "common_cli.h"

#include <multipass/cli/argparser.h>
#include <multipass/cli/client_platform.h>
#include <multipass/exceptions/cmd_exceptions.h>
#include <multipass/format.h>
#include <multipass/logging/log.h>

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>

namespace mp = multipass;
namespace mcp = multipass::cli::platform;
namespace mpl = multipass::logging;
namespace cmd = multipass::cmd;

namespace
{
constexpr auto category = "mount cmd";
const QString default_mount_type{"classic"};
const QString native_mount_type{"native"};

auto convert_id_for(const QString& id_string)
{
    bool ok;

    auto id = id_string.toUInt(&ok);
    if (!ok)
        throw std::runtime_error(id_string.toStdString() + " is an invalid id");

    return id;
}

auto checked_mount_type(const QString& type)
{
    if (type == default_mount_type)
        return mp::MountRequest_MountType_CLASSIC;

    if (type == native_mount_type)
        return mp::MountRequest_MountType_NATIVE;

    throw mp::ValidationException{fmt::format("Bad mount type '{}' specified, please use '{}' or '{}'", type,
                                              default_mount_type, native_mount_type)};
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

    auto streaming_callback = make_iterative_spinner_callback<MountRequest, MountReply>(spinner, *term);

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
                                  "If omitted, the mount point will be under /home/ubuntu/<source-dir>, "
                                  "where <source-dir> is the name of the <source> directory.",
                                  "<target> [<target> ...]");

    QCommandLineOption gid_mappings({"g", "gid-map"},
                                    "A mapping of group IDs for use in the mount. File and folder ownership will be "
                                    "mapped from <host> to <instance> inside the instance. Can be used multiple times. "
                                    "Mappings can only be specified as a one-to-one relationship.",
                                    "host>:<instance");
    QCommandLineOption uid_mappings({"u", "uid-map"},
                                    "A mapping of user IDs for use in the mount. File and folder ownership will be "
                                    "mapped from <host> to <instance> inside the instance. Can be used multiple times. "
                                    "Mappings can only be specified as a one-to-one relationship.",
                                    "host>:<instance");
    QCommandLineOption mount_type_option({"t", "type"},
                                         "Specify the type of mount to use.\n"
                                         "Classic mounts use technology built into Multipass.\n"
                                         "Native mounts use hypervisor and/or platform specific mounts.\n"
                                         "Valid types are: \'classic\' (default) and \'native\'",
                                         "type", default_mount_type);

    parser->addOptions({gid_mappings, uid_mappings, mount_type_option});

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

    request.clear_target_paths();
    for (auto i = 1; i < parser->positionalArguments().count(); ++i)
    {
        auto argument = parser->positionalArguments().at(i);
        auto instance_name = argument.section(':', 0, 0, QString::SectionSkipEmpty);
        auto target_path = argument.section(':', 1, -1, QString::SectionSkipEmpty);

        auto entry = request.add_target_paths();
        entry->set_instance_name(instance_name.toStdString());
        entry->set_target_path(target_path.toStdString());
    }

    QRegularExpression map_matcher("^([0-9]+[:][0-9]+)$");

    request.clear_mount_maps();
    auto mount_maps = request.mutable_mount_maps();

    if (parser->isSet(uid_mappings))
    {
        auto uid_maps = parser->values(uid_mappings);

        for (const auto& map : uid_maps)
        {
            if (!map_matcher.match(map).hasMatch())
            {
                cerr << "Invalid UID map given: " << map.toStdString() << "\n";
                return ParseCode::CommandLineError;
            }

            auto parsed_map = map.split(":");

            try
            {
                auto host_uid = convert_id_for(parsed_map.at(0));
                auto instance_uid = convert_id_for(parsed_map.at(1));

                auto uid_pair = mount_maps->add_uid_mappings();
                uid_pair->set_host_id(host_uid);
                uid_pair->set_instance_id(instance_uid);
            }
            catch (const std::exception& e)
            {
                cerr << e.what() << "\n";
                return ParseCode::CommandLineError;
            }
        }
    }
    else
    {
        mpl::log(mpl::Level::debug, category,
                 fmt::format("{}:{} {}(): adding default uid mapping", __FILE__, __LINE__, __FUNCTION__));

        auto uid_pair = mount_maps->add_uid_mappings();
        uid_pair->set_host_id(mcp::getuid());
        uid_pair->set_instance_id(mp::default_id);
    }

    if (parser->isSet(gid_mappings))
    {
        auto gid_maps = parser->values(gid_mappings);

        for (const auto& map : gid_maps)
        {
            if (!map_matcher.match(map).hasMatch())
            {
                cerr << "Invalid GID map given: " << map.toStdString() << "\n";
                return ParseCode::CommandLineError;
            }

            auto parsed_map = map.split(":");

            try
            {
                auto host_gid = convert_id_for(parsed_map.at(0));
                auto instance_gid = convert_id_for(parsed_map.at(1));

                auto gid_pair = mount_maps->add_gid_mappings();
                gid_pair->set_host_id(host_gid);
                gid_pair->set_instance_id(instance_gid);
            }
            catch (const std::exception& e)
            {
                cerr << e.what() << "\n";
                return ParseCode::CommandLineError;
            }
        }
    }
    else
    {
        mpl::log(mpl::Level::debug, category,
                 fmt::format("{}:{} {}(): adding default gid mapping", __FILE__, __LINE__, __FUNCTION__));

        auto gid_pair = mount_maps->add_gid_mappings();
        gid_pair->set_host_id(mcp::getgid());
        gid_pair->set_instance_id(mp::default_id);
    }

    try
    {
        request.set_mount_type(checked_mount_type(parser->value(mount_type_option).toLower()));
    }
    catch (mp::ValidationException& e)
    {
        cerr << "error: " << e.what() << "\n";
        return ParseCode::CommandLineError;
    }

    return ParseCode::Ok;
}
