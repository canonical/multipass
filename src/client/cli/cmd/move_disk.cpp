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

#include "move_disk.h"
#include "common_cli.h"

#include <multipass/cli/argparser.h>
#include <multipass/exceptions/cmd_exceptions.h>
#include <multipass/format.h>

namespace mp = multipass;
namespace cmd = multipass::cmd;

mp::ReturnCode cmd::MoveDisk::run(mp::ArgParser* parser)
{
    if (auto ret = parse_args(parser); ret != ParseCode::Ok)
    {
        return parser->returnCodeFrom(ret);
    }

    // First, list blocks to find the current attachment status
    auto on_list_success = [this](mp::ListBlocksReply& list_reply) {
        std::string current_instance;
        bool block_found = false;

        // Find the block device and check its current attachment
        for (const auto& block : list_reply.block_devices())
        {
            if (block.name() == block_name)
            {
                block_found = true;
                current_instance = block.attached_to();
                break;
            }
        }

        if (!block_found)
        {
            throw mp::ValidationException{
                fmt::format("Block device '{}' not found", block_name)};
        }

        // If already attached to the target instance, nothing to do
        if (current_instance == target_instance_name)
        {
            cout << fmt::format("Block device '{}' is already attached to instance '{}'\n",
                               block_name, target_instance_name);
            return ReturnCode::Ok;
        }

        // If attached to another instance, detach first
        if (!current_instance.empty())
        {
            detach_request.set_block_name(block_name);
            detach_request.set_instance_name(current_instance);

            auto on_detach_success = [this](mp::DetachBlockReply& detach_reply) {
                if (!detach_reply.error_message().empty())
                {
                    throw mp::ValidationException{
                        fmt::format("Failed to detach block device: {}", detach_reply.error_message())};
                }

                // Now attach to the target instance
                attach_request.set_block_name(block_name);
                attach_request.set_instance_name(target_instance_name);

                auto on_attach_success = [this](mp::AttachBlockReply& attach_reply) {
                    if (!attach_reply.error_message().empty())
                    {
                        throw mp::ValidationException{
                            fmt::format("Failed to attach block device: {}", attach_reply.error_message())};
                    }
                    return ReturnCode::Ok;
                };

                auto on_attach_failure = [](grpc::Status& status) {
                    throw mp::ValidationException{
                        fmt::format("Failed to connect to daemon: {}", status.error_message())};
                    return ReturnCode::CommandFail;
                };

                return dispatch(&RpcMethod::attach_block, attach_request, on_attach_success, on_attach_failure);
            };

            auto on_detach_failure = [](grpc::Status& status) {
                throw mp::ValidationException{
                    fmt::format("Failed to connect to daemon: {}", status.error_message())};
                return ReturnCode::CommandFail;
            };

            return dispatch(&RpcMethod::detach_block, detach_request, on_detach_success, on_detach_failure);
        }
        else
        {
            // Block is not attached, just attach to target instance
            attach_request.set_block_name(block_name);
            attach_request.set_instance_name(target_instance_name);

            auto on_attach_success = [this](mp::AttachBlockReply& attach_reply) {
                if (!attach_reply.error_message().empty())
                {
                    throw mp::ValidationException{
                        fmt::format("Failed to attach block device: {}", attach_reply.error_message())};
                }
                return ReturnCode::Ok;
            };

            auto on_attach_failure = [](grpc::Status& status) {
                throw mp::ValidationException{
                    fmt::format("Failed to connect to daemon: {}", status.error_message())};
                return ReturnCode::CommandFail;
            };

            return dispatch(&RpcMethod::attach_block, attach_request, on_attach_success, on_attach_failure);
        }
    };

    auto on_list_failure = [](grpc::Status& status) {
        throw mp::ValidationException{
            fmt::format("Failed to connect to daemon: {}", status.error_message())};
        return ReturnCode::CommandFail;
    };

    list_request.set_verbosity_level(parser->verbosityLevel());
    return dispatch(&RpcMethod::list_blocks, list_request, on_list_success, on_list_failure);
}

std::string cmd::MoveDisk::name() const
{
    return "move-disk";
}

QString cmd::MoveDisk::short_help() const
{
    return QStringLiteral("Move a block device to a VM instance");
}

QString cmd::MoveDisk::description() const
{
    return QStringLiteral("Move a block device to a VM instance. If the block device is already\n"
                          "attached to another VM, it will be automatically detached first and\n"
                          "then attached to the target VM. Both the source VM (if any) and target\n"
                          "VM must be in a stopped state.\n\n"
                          "Usage:\n"
                          "  multipass move-disk <block-device-name> <instance-name>");
}

mp::ParseCode cmd::MoveDisk::parse_args(mp::ArgParser* parser)
{
    parser->addPositionalArgument("block-device", "Name of the block device to move", "block-device");

    parser->addPositionalArgument("instance",
                                  "Name of the VM instance to move the block device to",
                                  "instance");

    auto status = parser->commandParse(this);

    if (status != ParseCode::Ok)
    {
        return status;
    }

    if (parser->positionalArguments().count() != 2)
    {
        throw mp::ValidationException{
            "move-disk requires exactly 2 arguments: <block-device-name> <instance-name>"};
    }

    const auto args = parser->positionalArguments();
    block_name = args.at(0).toStdString();
    target_instance_name = args.at(1).toStdString();

    return status;
}
