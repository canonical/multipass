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

#include "create_disk.h"
#include "common_cli.h"

#include <multipass/cli/argparser.h>
#include <multipass/memory_size.h>

namespace mp = multipass;
namespace cmd = multipass::cmd;

mp::ReturnCode cmd::CreateDisk::run(mp::ArgParser* parser)
{
    auto ret = parse_args(parser);
    if (ret != ParseCode::Ok)
    {
        return parser->returnCodeFrom(ret);
    }

    auto on_success = [this](mp::CreateDiskReply& reply) {
        cout << "Successfully created disk " << reply.disk_name() << " with size "
             << reply.disk_size() << "\n";
        return ReturnCode::Ok;
    };

    auto on_failure = [this](grpc::Status& status) {
        return standard_failure_handler_for(name(), cerr, status);
    };

    request.set_verbosity_level(parser->verbosityLevel());
    return dispatch(&RpcMethod::create_disk, request, on_success, on_failure);
}

std::string cmd::CreateDisk::name() const
{
    return "create-disk";
}

std::vector<std::string> cmd::CreateDisk::aliases() const
{
    return {"create-disk"};
}

QString cmd::CreateDisk::short_help() const
{
    return QStringLiteral("Create a new disk");
}

QString cmd::CreateDisk::description() const
{
    return QStringLiteral(
        "Create a new disk that can be attached to instances.\n\n"
        "The disk size can be specified with units (e.g., 10G for 10 gigabytes).");
}

mp::ParseCode cmd::CreateDisk::parse_args(mp::ArgParser* parser)
{
    QCommandLineOption nameOption("name", "Name for the disk", "name");
    QCommandLineOption sizeOption("size", "Size of the disk (e.g., 10G)", "size");

    parser->addOption(nameOption);
    parser->addOption(sizeOption);

    auto status = parser->commandParse(this);

    if (status != ParseCode::Ok)
    {
        return status;
    }

    if (parser->positionalArguments().count() > 0)
    {
        cerr << "This command takes no positional arguments\n";
        return ParseCode::CommandLineError;
    }

    std::string size_str;
    if (parser->isSet(sizeOption))
    {
        size_str = parser->value(sizeOption).toStdString();
    }
    else
    {
        size_str = "10G";
    }

    // Validate size using MemorySize
    mp::MemorySize{size_str}; // throw if bad

    // Check minimum size is 1G
    static const mp::MemorySize min_disk{"1G"};
    if (mp::MemorySize{size_str} < min_disk)
    {
        cerr << "Size must be at least 1G\n";
        return ParseCode::CommandLineError;
    }

    request.set_size(size_str);

    if (parser->isSet(nameOption))
    {
        request.set_name(parser->value(nameOption).toStdString());
    }
    // If name is not set, the server will generate one

    return status;
}
