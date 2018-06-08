/*
 * Copyright (C) 2018 Canonical, Ltd.
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

#include "copy_files.h"

#include <multipass/cli/argparser.h>
#include <multipass/cli/client_platform.h>
#include <multipass/ssh/scp_client.h>

#include <QDir>
#include <QFileInfo>

namespace mp = multipass;
namespace cmd = multipass::cmd;
namespace mcp = multipass::cli::platform;
using RpcMethod = mp::Rpc::Stub;

mp::ReturnCode cmd::CopyFiles::run(mp::ArgParser* parser)
{
    auto ret = parse_args(parser);
    if (ret != ParseCode::Ok)
    {
        return parser->returnCodeFrom(ret);
    }

    auto on_success = [this](mp::SSHInfoReply& reply) {
        // TODO: mainly for testing - need a better way to test parsing
        if (reply.ssh_info().empty())
            return ReturnCode::Ok;

        for (const auto& source : sources)
        {
            mp::SSHInfo ssh_info;
            if (!source.first.empty())
            {
                ssh_info = reply.ssh_info().find(source.first)->second;
            }
            else if (!destination.first.empty())
            {
                ssh_info = reply.ssh_info().find(destination.first)->second;
            }

            auto host = ssh_info.host();
            auto port = ssh_info.port();
            auto username = ssh_info.username();
            auto priv_key_blob = ssh_info.priv_key_base64();

            auto destination_path{destination.second};
            if (destination_path.empty())
            {
                destination_path.append(QFileInfo(QString::fromStdString(source.second)).fileName().toStdString());
            }
            else if (QFileInfo(QString::fromStdString(destination_path)).isDir())
            {
                destination_path.append("/" +
                                        QFileInfo(QString::fromStdString(source.second)).fileName().toStdString());
            }

            try
            {
                mp::SCPClient scp_client{host, port, username, priv_key_blob};
                if (!destination.first.empty())
                    scp_client.push_file(source.second, destination_path);
                else
                    scp_client.pull_file(source.second, destination_path);
            }
            catch (const std::exception& e)
            {
                cerr << "copy-files failed: " << e.what() << "\n";
                return ReturnCode::CommandFail;
            }
        }
        return ReturnCode::Ok;
    };

    auto on_failure = [this](grpc::Status& status) {
        cerr << "copy-files failed: " << status.error_message() << "\n";

        return ReturnCode::CommandFail;
    };

    return dispatch(&RpcMethod::ssh_info, request, on_success, on_failure);
}

std::string cmd::CopyFiles::name() const
{
    return "copy-files";
}

QString cmd::CopyFiles::short_help() const
{
    return QStringLiteral("Copy files between the host and instances");
}

QString cmd::CopyFiles::description() const
{
    // TODO: Don't mention directories until we support that
    // return QStringLiteral("Copy files and directories between the host and instances.");
    return QStringLiteral("Copy files between the host and instances.");
}

mp::ParseCode cmd::CopyFiles::parse_args(mp::ArgParser* parser)
{
    parser->addPositionalArgument("source", "One or more paths to copy, prefixed with <name:> "
                                            "for paths inside the instance",
                                  "<source> [<source> ...]");
    parser->addPositionalArgument("destination", "The destination path, prefixed with <name:> for "
                                                 "a path inside the instance",
                                  "<destination>");

    auto status = parser->commandParse(this);
    if (status != ParseCode::Ok)
        return status;

    if (parser->positionalArguments().count() < 2)
    {
        cerr << "Not enough arguments given\n";
        return ParseCode::CommandLineError;
    }

    for (auto i = 0; i < parser->positionalArguments().count() - 1; ++i)
    {
        auto source_entry = parser->positionalArguments().at(i);
        QString source_path, instance_name;

        mcp::parse_copy_files_entry(source_entry, source_path, instance_name);

        if (source_path.isEmpty())
        {
            cerr << "Invalid source path given\n";
            return ParseCode::CommandLineError;
        }

        if (instance_name.isEmpty())
        {
            QFileInfo source(source_path);
            if (!source.exists())
            {
                cerr << "Source path \"" << source_path.toStdString() << "\" does not exist\n";
                return ParseCode::CommandLineError;
            }

            if (!source.isFile())
            {
                cerr << "Source path must be a file\n";
                return ParseCode::CommandLineError;
            }

            if (!source.isReadable())
            {
                cerr << "Source path \"" << source_path.toStdString() << "\" is not readable\n";
                return ParseCode::CommandLineError;
            }
        }
        else
        {
            auto entry = request.add_instance_name();
            entry->append(instance_name.toStdString());
        }

        sources.emplace_back(instance_name.toStdString(), source_path.toStdString());
    }

    auto destination_entry = parser->positionalArguments().last();
    QString destination_path, instance_name;

    mcp::parse_copy_files_entry(destination_entry, destination_path, instance_name);
    if (instance_name.isEmpty())
    {
        QFileInfo destination(destination_path);

        if (destination.isDir())
        {
            if (!destination.isWritable())
            {
                cerr << "Destination path \"" << destination_path.toStdString() << "\" is not writable\n";
                return ParseCode::CommandLineError;
            }
        }
        else
        {
            if (!QFileInfo(destination.dir().absolutePath()).isWritable())
            {
                cerr << "Destination path \"" << destination_path.toStdString() << "\" is not writable\n";
                return ParseCode::CommandLineError;
            }
            else if (sources.size() > 1)
            {
                cerr << "Destination path must be a directory\n";
                return ParseCode::CommandLineError;
            }
        }
    }
    else
    {
        if (!request.instance_name().empty())
        {
            cerr << "Cannot specify an instance name for both source and destination\n";
            return ParseCode::CommandLineError;
        }

        auto entry = request.add_instance_name();
        entry->append(instance_name.toStdString());
    }

    if (request.instance_name().empty())
    {
        cerr << "An instance name is needed for either source or destination\n";
        return ParseCode::CommandLineError;
    }

    destination = std::make_pair(instance_name.toStdString(), destination_path.toStdString());

    return ParseCode::Ok;
}
