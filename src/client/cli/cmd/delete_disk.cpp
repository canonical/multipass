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

#include "delete_disk.h"
#include "common_cli.h"

#include <multipass/cli/argparser.h>
#include <multipass/exceptions/cmd_exceptions.h>
#include <multipass/format.h>

namespace mp = multipass;
namespace cmd = multipass::cmd;

mp::ReturnCode cmd::DeleteDisk::run(mp::ArgParser* parser)
{
    if (auto ret = parse_args(parser); ret != ParseCode::Ok)
    {
        return parser->returnCodeFrom(ret);
    }

    // First, list all block devices to find the disk and check if it's attached
    auto on_list_success = [this](mp::ListBlocksReply& reply) {
        std::string attached_instance;
        bool disk_found = false;

        for (const auto& block_device : reply.block_devices())
        {
            if (block_device.name() == disk_name)
            {
                disk_found = true;
                if (!block_device.attached_to().empty())
                {
                    attached_instance = block_device.attached_to();
                }
                break;
            }
        }

        if (!disk_found)
        {
            throw mp::ValidationException{fmt::format("Block device '{}' not found", disk_name)};
        }

        // If the disk is attached, detach it first
        if (!attached_instance.empty())
        {
            detach_request.set_block_name(disk_name);
            detach_request.set_instance_name(attached_instance);

            auto on_detach_success = [this](mp::DetachBlockReply& detach_reply) {
                if (!detach_reply.error_message().empty())
                {
                    throw mp::ValidationException{
                        fmt::format("Failed to detach block device '{}': {}", disk_name, detach_reply.error_message())};
                }

                // Now delete the disk
                delete_request.set_name(disk_name);

                auto on_delete_success = [this](mp::DeleteBlockReply& delete_reply) {
                    if (!delete_reply.error_message().empty())
                    {
                        throw mp::ValidationException{
                            fmt::format("Failed to delete block device '{}': {}", disk_name, delete_reply.error_message())};
                    }
                    return ReturnCode::Ok;
                };

                auto on_delete_failure = [](grpc::Status& status) {
                    throw mp::ValidationException{
                        fmt::format("Failed to connect to daemon: {}", status.error_message())};
                    return ReturnCode::CommandFail;
                };

                return dispatch(&RpcMethod::delete_block, delete_request, on_delete_success, on_delete_failure);
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
            // Disk is not attached, delete it directly
            delete_request.set_name(disk_name);

            auto on_delete_success = [this](mp::DeleteBlockReply& delete_reply) {
                if (!delete_reply.error_message().empty())
                {
                    throw mp::ValidationException{
                        fmt::format("Failed to delete block device '{}': {}", disk_name, delete_reply.error_message())};
                }
                return ReturnCode::Ok;
            };

            auto on_delete_failure = [](grpc::Status& status) {
                throw mp::ValidationException{
                    fmt::format("Failed to connect to daemon: {}", status.error_message())};
                return ReturnCode::CommandFail;
            };

            return dispatch(&RpcMethod::delete_block, delete_request, on_delete_success, on_delete_failure);
        }
    };

    auto on_list_failure = [](grpc::Status& status) {
        throw mp::ValidationException{
            fmt::format("Failed to connect to daemon: {}", status.error_message())};
        return ReturnCode::CommandFail;
    };

    return dispatch(&RpcMethod::list_blocks, list_request, on_list_success, on_list_failure);
}

std::string cmd::DeleteDisk::name() const
{
    return "delete-disk";
}

QString cmd::DeleteDisk::short_help() const
{
    return QStringLiteral("Delete a block device, automatically detaching if attached");
}

QString cmd::DeleteDisk::description() const
{
    return QStringLiteral("Delete a block device. If the block device is currently\n"
                          "attached to a VM instance, it will be automatically\n"
                          "detached first and then deleted. The VM must be in a\n"
                          "stopped state for detachment to succeed.");
}

mp::ParseCode cmd::DeleteDisk::parse_args(mp::ArgParser* parser)
{
    parser->addPositionalArgument("name", "Name of the block device to delete", "name");

    auto status = parser->commandParse(this);

    if (status != ParseCode::Ok)
    {
        return status;
    }

    if (parser->positionalArguments().count() != 1)
    {
        throw mp::ValidationException{fmt::format(
            "Wrong number of arguments given. Expected 1 (<block device name>)")};
    }

    const auto args = parser->positionalArguments();
    disk_name = args.at(0).toStdString();

    return status;
}
