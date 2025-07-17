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

#include "block_create.h"
#include "common_cli.h"

#include <multipass/cli/argparser.h>
#include <multipass/exceptions/cmd_exceptions.h>
#include <multipass/format.h>
#include <multipass/memory_size.h>

namespace mp = multipass;
namespace cmd = multipass::cmd;

namespace
{
constexpr auto default_block_size = "10G";
constexpr auto min_block_size = "1G";
} // namespace

mp::ReturnCode cmd::BlockCreate::run(mp::ArgParser* parser)
{
    if (auto ret = parse_args(parser); ret != ParseCode::Ok)
    {
        return parser->returnCodeFrom(ret);
    }

    auto on_success = [](mp::CreateBlockReply& reply) {
        if (!reply.error_message().empty())
        {
            throw mp::ValidationException{
                fmt::format("Failed to create block device: {}", reply.error_message())};
        }
        return ReturnCode::Ok;
    };

    auto on_failure = [](grpc::Status& status) {
        throw mp::ValidationException{
            fmt::format("Failed to connect to daemon: {}", status.error_message())};
        return ReturnCode::CommandFail;
    };

    return dispatch(&RpcMethod::create_block, request, on_success, on_failure);
}

std::string cmd::BlockCreate::name() const
{
    return "block-create";
}

QString cmd::BlockCreate::short_help() const
{
    return QStringLiteral("Create a new block device");
}

QString cmd::BlockCreate::description() const
{
    return QStringLiteral("Create a new block device that can be attached to VMs.");
}

mp::ParseCode cmd::BlockCreate::parse_args(mp::ArgParser* parser)
{
    parser->addPositionalArgument("name", "Name of the block device to create.", "name");

    QCommandLineOption sizeOption(
        {"s", "size"},
        QString::fromStdString(fmt::format("Size of block device to create. "
                                           "Positive integers, in bytes, or with K, M, G suffix."
                                           "\nMinimum: {}, default: {}.",
                                           min_block_size,
                                           default_block_size)),
        "size",
        QString::fromUtf8(default_block_size));

    parser->addOption(sizeOption);

    auto status = parser->commandParse(this);

    if (status != ParseCode::Ok)
    {
        return status;
    }

    if (parser->positionalArguments().count() != 1)
    {
        throw mp::ValidationException{
            "block-create requires one argument: the name of the block device"};
    }

    request.set_name(parser->positionalArguments().first().toStdString());

    if (parser->isSet(sizeOption))
    {
        auto size_str = parser->value(sizeOption).toStdString();
        try
        {
            auto size = mp::MemorySize{size_str};
            if (size < mp::MemorySize{min_block_size})
            {
                throw mp::ValidationException{
                    fmt::format("Block device size '{}' is too small, minimum size is {}",
                                size_str,
                                min_block_size)};
            }
            request.set_size(size_str);
        }
        catch (const std::invalid_argument&)
        {
            throw mp::ValidationException{fmt::format(
                "Invalid block device size '{}', must be a positive number with K, M, or G suffix",
                size_str)};
        }
    }
    else
    {
        request.set_size(default_block_size);
    }

    return status;
}
