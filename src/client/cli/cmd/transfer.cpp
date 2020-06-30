/*
 * Copyright (C) 2018-2020 Canonical, Ltd.
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

#include "transfer.h"
#include "common_cli.h"

#include <multipass/cli/argparser.h>
#include <multipass/cli/client_platform.h>
#include <multipass/ssh/sftp_client.h>

#include <QFileInfo>

namespace mp = multipass;
namespace cmd = multipass::cmd;
namespace mcp = multipass::cli::platform;
using RpcMethod = mp::Rpc::Stub;

namespace
{
const char streaming_symbol{'-'};
} // namespace

mp::ReturnCode cmd::Transfer::run(mp::ArgParser* parser)
{
    streaming_enabled = false;
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

            try
            {
                mp::SFTPClient sftp_client{host, port, username, priv_key_blob};

                if (streaming_enabled)
                {
                    if (destination.first.empty())
                        sftp_client.stream_file(source.second, term->cout());
                    else
                        sftp_client.stream_file(destination.second, term->cin());
                }
                else
                {
                    if (!destination.first.empty())
                        sftp_client.push_file(source.second, destination.second);
                    else
                        sftp_client.pull_file(source.second, destination.second);
                }
            }
            catch (const std::exception& e)
            {
                cerr << "transfer failed: " << e.what() << "\n";
                return ReturnCode::CommandFail;
            }
        }
        return ReturnCode::Ok;
    };

    auto on_failure = [this](grpc::Status& status) { return standard_failure_handler_for(name(), cerr, status); };

    request.set_verbosity_level(parser->verbosityLevel());
    return dispatch(&RpcMethod::ssh_info, request, on_success, on_failure);
}

std::string cmd::Transfer::name() const
{
    return "transfer";
}

std::vector<std::string> cmd::Transfer::aliases() const
{
    return {name(), "copy-files"};
}

QString cmd::Transfer::short_help() const
{
    return QStringLiteral("Transfer files between the host and instances");
}

QString cmd::Transfer::description() const
{
    // TODO: Don't mention directories until we support that
    // return QStringLiteral("Copy files and directories between the host and instances.");
    return QStringLiteral("Copy files between the host and instances.");
}

mp::ParseCode cmd::Transfer::parse_args(mp::ArgParser* parser)
{
    parser->addPositionalArgument("source",
                                  "One or more paths to transfer, prefixed with <name:> "
                                  "for paths inside the instance, or '-' for stdin",
                                  "<source> [<source> ...]");
    parser->addPositionalArgument("destination",
                                  "The destination path, prefixed with <name:> for "
                                  "a path inside the instance, or '-' for stdout",
                                  "<destination>");

    auto status = parser->commandParse(this);
    if (status != ParseCode::Ok)
        return status;

    if (parser->positionalArguments().count() < 2)
    {
        cerr << "Not enough arguments given\n";
        return ParseCode::CommandLineError;
    }

    const auto& args = parser->positionalArguments();
    const auto num_streaming_symbols = std::count(std::begin(args), std::end(args), streaming_symbol);
    const bool allow_streaming = (args.count() == 2);

    if (num_streaming_symbols && !allow_streaming)
    {
        cerr << fmt::format("Only two arguments allowed when using '{}'\n", streaming_symbol);
        return ParseCode::CommandLineError;
    }

    if (num_streaming_symbols > 1)
    {
        cerr << fmt::format("Only one '{}'\n", streaming_symbol);
        return ParseCode::CommandLineError;
    }

    const auto source_code = parse_sources(parser);
    if (ParseCode::Ok != source_code)
        return source_code;

    const auto destination_code = parse_destination(parser);
    if (ParseCode::Ok != destination_code)
        return destination_code;

    if (request.instance_name().empty())
    {
        cerr << "An instance name is needed for either source or destination\n";
        return ParseCode::CommandLineError;
    }

    return ParseCode::Ok;
}

mp::ParseCode cmd::Transfer::parse_sources(mp::ArgParser* parser)
{
    for (auto i = 0; i < parser->positionalArguments().count() - 1; ++i)
    {
        auto source_entry = parser->positionalArguments().at(i);
        QString source_path, instance_name;

        mcp::parse_transfer_entry(source_entry, source_path, instance_name);

        if (source_path.isEmpty())
        {
            cerr << "Invalid source path given\n";
            return ParseCode::CommandLineError;
        }

        if (streaming_symbol == source_path)
        {
            streaming_enabled = true;
            sources.emplace_back(instance_name.toStdString(), "");
            return ParseCode::Ok;
        }

        if (instance_name.isEmpty())
        {
            QFileInfo source(source_path);
            if (!source.exists())
            {
                cerr << fmt::format("Source path \"{}\" does not exist\n", source_path);
                return ParseCode::CommandLineError;
            }

            if (!source.isFile())
            {
                cerr << "Source path must be a file\n";
                return ParseCode::CommandLineError;
            }

            if (!source.isReadable())
            {
                cerr << fmt::format("Source path \"{}\" is not readable\n", source_path);
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

    return ParseCode::Ok;
}

mp::ParseCode cmd::Transfer::parse_destination(mp::ArgParser* parser)
{
    auto destination_entry = parser->positionalArguments().last();
    QString destination_path, instance_name;

    mcp::parse_transfer_entry(destination_entry, destination_path, instance_name);
    if (streaming_symbol == destination_path)
    {
        streaming_enabled = true;
        destination = std::make_pair("", "");
        return ParseCode::Ok;
    }

    if (instance_name.isEmpty())
    {
        QFileInfo destination_file(destination_path);

        if (destination_file.isDir())
        {
            if (!destination_file.isWritable())
            {
                cerr << fmt::format("Destination path \"{}\" is not writable\n", destination_path);
                return ParseCode::CommandLineError;
            }
        }
        else
        {
            if (!QFileInfo(destination_file.dir().absolutePath()).isWritable())
            {
                cerr << fmt::format("Destination path \"{}\" is not writable\n", destination_path);
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

    destination = std::make_pair(instance_name.toStdString(), destination_path.toStdString());
    return ParseCode::Ok;
}
