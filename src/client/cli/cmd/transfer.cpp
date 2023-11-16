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

#include "transfer.h"
#include "common_cli.h"

#include <multipass/cli/argparser.h>
#include <multipass/cli/client_platform.h>
#include <multipass/file_ops.h>
#include <multipass/ssh/sftp_utils.h>

#include <fmt/std.h>

#include <QRegularExpression>

namespace mp = multipass;
namespace cmd = multipass::cmd;
namespace mcp = multipass::cli::platform;
namespace fs = mp::fs;

namespace
{
constexpr char streaming_symbol{'-'};
} // namespace

mp::ReturnCode cmd::Transfer::run(mp::ArgParser* parser)
{
    auto ret = parse_args(parser);
    if (ret != ParseCode::Ok)
    {
        return parser->returnCodeFrom(ret);
    }

    auto on_success = [this](mp::SSHInfoReply& reply) {
        auto success = true;
        for (const auto& [instance_name, ssh_info] : reply.ssh_info())
        {
            try
            {
                auto sftp_client = MP_SFTPUTILS.make_SFTPClient(ssh_info.host(), ssh_info.port(), ssh_info.username(),
                                                                ssh_info.priv_key_base64());

                if (const auto args = std::get_if<InstanceSourcesLocalTarget>(&arguments); args)
                {
                    auto& [sources, target] = *args;
                    std::error_code err;
                    if (sources.size() > 1 && !MP_FILEOPS.is_directory(target, err) && !err)
                        throw std::runtime_error{fmt::format("Target {:?} is not a directory", target)};
                    else if (err)
                        throw std::runtime_error{fmt::format("Cannot access {:?}: {}", target, err.message())};
                    for (auto [s, s_end] = sources.equal_range(instance_name); s != s_end; ++s)
                        success &= sftp_client->pull(s->second, target, flags);
                }

                if (const auto args = std::get_if<LocalSourcesInstanceTarget>(&arguments); args)
                {
                    auto& [sources, target] = *args;
                    if (sources.size() > 1 && !sftp_client->is_remote_dir(target))
                        throw std::runtime_error{fmt::format("Target {:?} is not a directory", target)};
                    for (const auto& source : sources)
                        success &= sftp_client->push(source, target, flags);
                }

                if (const auto args = std::get_if<FromCin>(&arguments); args)
                    sftp_client->from_cin(term->cin(), args->target, flags.testFlag(SFTPClient::Flag::MakeParent));

                if (const auto args = std::get_if<ToCout>(&arguments); args)
                    sftp_client->to_cout(args->source, term->cout());
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
    parser->addOption({{"r", "recursive"}, "Recursively copy entire directories"});
    parser->addOption({{"p", "parents"}, "Make parent directories as needed"});

    if (auto status = parser->commandParse(this); status != ParseCode::Ok)
        return status;

    flags.setFlag(SFTPClient::Flag::Recursive, parser->isSet("r"));
    flags.setFlag(SFTPClient::Flag::MakeParent, parser->isSet("p"));

    auto positionalArgs = parser->positionalArguments();
    if (positionalArgs.size() < 2)
    {
        term->cerr() << "Not enough arguments given\n";
        return ParseCode::CommandLineError;
    }

    auto split_args = args_to_instance_and_path(positionalArgs);
    auto split_target = std::move(split_args.back());
    split_args.pop_back();
    auto& split_sources = split_args;

    const auto full_target = positionalArgs.takeLast();
    const auto& full_sources = positionalArgs;

    if (const auto ret_opt = parse_streaming(full_sources, full_target, split_sources, split_target); ret_opt)
        return ret_opt.value();

    return parse_non_streaming(split_sources, split_target);
}

std::vector<std::pair<std::string, fs::path>> cmd::Transfer::args_to_instance_and_path(const QStringList& args)
{
    std::vector<std::pair<std::string, fs::path>> instance_entry_args;
    instance_entry_args.reserve(args.size());
    for (const auto& entry : args)
    {
        // this is needed so that Windows absolute paths are not split at the colon following the drive letter
        if (QRegularExpression{R"(^[A-Za-z]:\\.*)"}.match(entry).hasMatch())
        {
            instance_entry_args.emplace_back("", entry.toStdString());
            continue;
        }

        const auto source_components = entry.split(':');
        const auto instance_name = source_components.size() == 1 ? "" : source_components.first().toStdString();
        const auto file_path = source_components.size() == 1 ? source_components.first() : entry.section(':', 1);
        if (!instance_name.empty())
            request.add_instance_name()->append(instance_name);
        instance_entry_args.emplace_back(instance_name, file_path.isEmpty() ? "." : file_path.toStdString());
    }
    return instance_entry_args;
}

std::optional<mp::ParseCode>
multipass::cmd::Transfer::parse_streaming(const QStringList& full_sources, const QString& full_target,
                                          std::vector<std::pair<std::string, fs::path>> split_sources,
                                          std::pair<std::string, fs::path> split_target)
{
    const auto source_streaming = std::count(full_sources.begin(), full_sources.end(), streaming_symbol);
    const auto target_streaming = full_target == streaming_symbol;
    if ((target_streaming && source_streaming) || source_streaming > 1)
    {
        term->cerr() << fmt::format("Only one '{}' allowed\n", streaming_symbol);
        return ParseCode::CommandLineError;
    }

    if ((target_streaming || source_streaming) && full_sources.size() > 1)
    {
        term->cerr() << fmt::format("Only two arguments allowed when using '{}'\n", streaming_symbol);
        return ParseCode::CommandLineError;
    }

    if (target_streaming)
    {
        if (split_sources.front().first.empty())
        {
            term->cerr() << "Source must be from inside an instance\n";
            return ParseCode::CommandLineError;
        }

        arguments = ToCout{std::move(split_sources.front().second)};
        return ParseCode::Ok;
    }

    if (source_streaming)
    {
        if (split_target.first.empty())
        {
            term->cerr() << "Target must be inside an instance\n";
            return ParseCode::CommandLineError;
        }

        arguments = FromCin{std::move(split_target).second};
        return ParseCode::Ok;
    }

    return std::nullopt;
}

mp::ParseCode cmd::Transfer::parse_non_streaming(std::vector<std::pair<std::string, fs::path>>& split_sources,
                                                 std::pair<std::string, fs::path>& split_target)
{
    if (split_target.first.empty())
    {
        if (request.instance_name_size() == (int)split_sources.size())
        {
            arguments = InstanceSourcesLocalTarget{{split_sources.begin(), split_sources.end()},
                                                   std::move(split_target.second)};
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
    source_paths.reserve(split_sources.size());
    std::transform(split_sources.begin(), split_sources.end(), std::back_inserter(source_paths),
                   [](auto& item) { return std::move(item.second); });
    arguments = LocalSourcesInstanceTarget{std::move(source_paths), std::move(split_target.second)};
    return ParseCode::Ok;
}
