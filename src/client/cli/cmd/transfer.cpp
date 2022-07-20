/*
 * Copyright (C) 2018-2022 Canonical, Ltd.
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
#include <multipass/file_ops.h>
#include <multipass/ssh/sftp_utils.h>

namespace mp = multipass;
namespace cmd = multipass::cmd;
namespace mcp = multipass::cli::platform;

namespace
{
constexpr char streaming_symbol{'-'};
} // namespace

mp::ReturnCode cmd::Transfer::run(mp::ArgParser* parser)
{
    auto ret = parse_args(parser);
    if (ret != ParseCode::Ok)
        return parser->returnCodeFrom(ret);

    auto on_success = [this](mp::SSHInfoReply& reply) {
        auto success = true;
        for (const auto& [instance_name, ssh_info] : reply.ssh_info())
        {
            try
            {
                auto sftp_client = MP_SFTPUTILS.make_SFTPClient(ssh_info.host(), ssh_info.port(), ssh_info.username(),
                                                                ssh_info.priv_key_base64());
                switch (arguments.index())
                {
                case INSTANCE_SOURCES_LOCAL_TARGET:
                {
                    auto& [sources, target] = std::get<INSTANCE_SOURCES_LOCAL_TARGET>(arguments);
                    std::error_code err;
                    if (sources.size() > 1 && !MP_FILEOPS.is_directory(target, err) && !err)
                        throw std::runtime_error{fmt::format("[sftp] target {} is not a directory", target)};
                    else if (err)
                        throw std::runtime_error{fmt::format("[sftp] cannot access {}: {}", target, err.message())};
                    for (auto [s, s_end] = sources.equal_range(instance_name); s != s_end; ++s)
                        success &= sftp_client->pull(s->second, target, flags, term->cerr());
                    break;
                }
                case LOCAL_SOURCES_INSTANCE_TARGET:
                {
                    auto& [sources, target] = std::get<LOCAL_SOURCES_INSTANCE_TARGET>(arguments);
                    if (sources.size() > 1 && !sftp_client->is_dir(target))
                        throw std::runtime_error{fmt::format("[sftp] target {} is not a directory", target)};
                    for (const auto& source : sources)
                        success &= sftp_client->push(source, target, flags, term->cerr());
                    break;
                }
                case FROM_CIN:
                    sftp_client->from_cin(term->cin(), std::get<FROM_CIN>(arguments).target);
                    break;
                case TO_COUT:
                    sftp_client->to_cout(std::get<TO_COUT>(arguments).source, term->cout());
                    break;
                }
            }
            catch (const std::exception& e)
            {
                term->cerr() << e.what() << "\n";
                success = false;
            }
        }

        return success ? ReturnCode::Ok : ReturnCode::CommandFail;
    };

    auto on_failure = [this](grpc::Status& status) {
        return standard_failure_handler_for(name(), term->cerr(), status);
    };

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
    return QStringLiteral("Copy files and directories between the host and instances.");
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
    parser->addOption({"r", "Recursively copy entire directories"});

    if (auto status = parser->commandParse(this); status != ParseCode::Ok)
        return status;

    flags.setFlag(TransferFlags::Recursive, parser->isSet("r"));

    auto positionalArgs = parser->positionalArguments();
    if (positionalArgs.size() < 2)
    {
        term->cerr() << "Not enough arguments given\n";
        return ParseCode::CommandLineError;
    }

    std::vector<std::pair<std::string, fs::path>> instance_entry_args;
    instance_entry_args.reserve(positionalArgs.size());
    for (const auto& entry : positionalArgs)
    {
        const auto source_components = entry.split(':');
        const auto instance_name = source_components.size() == 1 ? "" : source_components.first().toStdString();
        const auto file_path = source_components.size() == 1 ? source_components.first() : source_components.at(1);
        if (!instance_name.empty())
            request.add_instance_name()->append(instance_name);
        instance_entry_args.emplace_back(instance_name, file_path.isEmpty() ? "." : file_path.toStdString());
    }

    auto instance_target = std::move(instance_entry_args.back());
    instance_entry_args.pop_back();
    auto& instance_sources = instance_entry_args;

    const auto target = positionalArgs.takeLast();
    const auto& sources = positionalArgs;

    const auto source_streaming = std::count(sources.begin(), sources.end(), streaming_symbol);
    const auto target_streaming = target == streaming_symbol;
    if ((target_streaming && source_streaming) || source_streaming > 1)
    {
        term->cerr() << fmt::format("Only one '{}' allowed\n", streaming_symbol);
        return ParseCode::CommandLineError;
    }

    if ((target_streaming || source_streaming) && sources.size() > 1)
    {
        term->cerr() << fmt::format("Only two arguments allowed when using '{}'\n", streaming_symbol);
        return ParseCode::CommandLineError;
    }

    if (target_streaming)
    {
        if (instance_sources.front().first.empty())
        {
            term->cerr() << "Source must be from inside an instance\n";
            return ParseCode::CommandLineError;
        }

        arguments = ToCout{std::move(instance_sources.front().second)};
        return ParseCode::Ok;
    }

    if (source_streaming)
    {
        if (instance_target.first.empty())
        {
            term->cerr() << "Target must be inside an instance\n";
            return ParseCode::CommandLineError;
        }

        arguments = FromCin{std::move(instance_target).second};
        return ParseCode::Ok;
    }

    if (instance_target.first.empty())
    {
        if (request.instance_name_size() == (int)instance_sources.size())
        {
            arguments = InstanceSourcesLocalTarget{{instance_sources.begin(), instance_sources.end()},
                                                   std::move(instance_target.second)};
            return ParseCode::Ok;
        }

        term->cerr() << (request.instance_name_size()
                             ? "All sources must be from inside an instance\n"
                             : "An instance name is needed for either source or destination\n");
        return ParseCode::CommandLineError;
    }

    if (request.instance_name_size() > 1)
    {
        term->cerr() << "Cannot specify an instance name for both source and destination\n";
        return ParseCode::CommandLineError;
    }

    std::vector<fs::path> source_paths;
    source_paths.reserve(instance_sources.size());
    std::transform(instance_sources.begin(), instance_sources.end(), std::back_inserter(source_paths),
                   [](auto& item) { return std::move(item.second); });
    arguments = LocalSourcesInstanceTarget{std::move(source_paths), std::move(instance_target.second)};
    return ParseCode::Ok;
}
