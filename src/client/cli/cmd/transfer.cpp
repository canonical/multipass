/*
 * Copyright (C) 2018-2019 Canonical, Ltd.
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
#include <multipass/ssh/scp_client.h>

#include <QDir>
#include <QFileInfo>
#include <QTemporaryFile>

namespace mp = multipass;
namespace cmd = multipass::cmd;
namespace mcp = multipass::cli::platform;
using RpcMethod = mp::Rpc::Stub;

namespace
{
const char* teplate_mask{"temp"};
const char* template_symbol{"-"};
const char* stdin_path {"/dev/stdin"};
const char* stdout_path{"/dev/stdout"};
}

mp::ReturnCode cmd::Transfer::run(mp::ArgParser* parser)
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

        // NOTE: need temp file before pushing copy to an instance, because we need to know size of file
        QTemporaryFile temp_file;
        if (sources.size() == 1)
        {
            auto& source = sources.front();
            if (source.second == stdin_path)
            {
                if (stdin_path == source.second)
                {
                    temp_file.setFileTemplate(teplate_mask);
                    if (!temp_file.open())
                        return ReturnCode::CommandFail;

                    QTextStream in_stream(stdin);
                    QTextStream out_stream(&temp_file);

                    while (!in_stream.atEnd())
                        out_stream << in_stream.readAll();

                    source.second = temp_file.fileName().toStdString();
                }

            }
        }

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
                mp::SCPClient scp_client{host, port, username, priv_key_blob};
                if (!destination.first.empty())
                    scp_client.push_file(source.second, destination.second);
                else
                    scp_client.pull_file(source.second, destination.second);
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
    parser->addPositionalArgument("source", "One or more paths to transfer, prefixed with <name:> "
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

    const bool allow_templates = (parser->positionalArguments().count() == 2);
    if (allow_templates)
    {
        const auto arguments = parser->positionalArguments();
        const auto is_valid_template = !std::all_of(arguments.begin(), arguments.end(), [](const QString& argument)
        {
            return (template_symbol == argument);
        });

        if (!is_valid_template)
        {
            cerr << "Only one occurence of template allowed\n";
            return ParseCode::CommandLineError;
        }
    }

    const auto source_code = parse_sources(parser, allow_templates);
    if (ParseCode::Ok != source_code)
        return source_code;

    const auto destination_code = parse_destination(parser, allow_templates);
    if (ParseCode::Ok != destination_code)
        return destination_code;

    if (request.instance_name().empty())
    {
        cerr << "An instance name is needed for either source or destination\n";
        return ParseCode::CommandLineError;
    }

    return ParseCode::Ok;
}

multipass::ParseCode cmd::CopyFiles::parse_sources(multipass::ArgParser *parser, bool allow_templates)
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

        if (allow_templates && template_symbol == source_path)
        {
            sources.emplace_back("", stdin_path);
            return ParseCode::Ok;
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

    return ParseCode::Ok;
}

multipass::ParseCode cmd::CopyFiles::parse_destination(multipass::ArgParser *parser, bool allow_templates)
{
    auto destination_entry = parser->positionalArguments().last();
    QString destination_path, instance_name;

    mcp::parse_transfer_entry(destination_entry, destination_path, instance_name);
    if (allow_templates && template_symbol == destination_path)
    {
        destination = std::make_pair("", stdout_path);
        return ParseCode::Ok;
    }

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

    destination = std::make_pair(instance_name.toStdString(), destination_path.toStdString());
    return ParseCode::Ok;
}
