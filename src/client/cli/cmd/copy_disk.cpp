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

#include "copy_disk.h"
#include "common_cli.h"

#include <multipass/cli/argparser.h>
#include <multipass/exceptions/cmd_exceptions.h>
#include <multipass/format.h>

#include <QRandomGenerator>
#include <QUuid>

namespace mp = multipass;
namespace cmd = multipass::cmd;

namespace
{
// Generate copy names in format: {source-name}-copy-{xx} where xx is two random letters/numbers
QString generate_copy_disk_name(const QString& source_name, const std::function<bool(const QString&)>& name_exists_check)
{
    // Generate copy names in format: {source-name}-copy-{xx} where xx is two random letters/numbers
    const QString letters = "abcdefhijkmnpqrstuvwxyz";
    const QString alphanumeric = "abcdefghijkmnopqrstuvwxyz0123456789";

    for (int attempts = 0; attempts < 1000; ++attempts)
    {
        QString copy_id;

        // Ensure at least one character is a letter
        if (QRandomGenerator::global()->bounded(2) == 0)
        {
            // First char is letter, second can be anything
            copy_id += letters[QRandomGenerator::global()->bounded(letters.length())];
            copy_id += alphanumeric[QRandomGenerator::global()->bounded(alphanumeric.length())];
        }
        else
        {
            // Second char is letter, first can be anything
            copy_id += alphanumeric[QRandomGenerator::global()->bounded(alphanumeric.length())];
            copy_id += letters[QRandomGenerator::global()->bounded(letters.length())];
        }

        QString copy_name = QString("%1-copy-%2").arg(source_name, copy_id);

        if (!name_exists_check(copy_name))
        {
            return copy_name;
        }
    }

    // Fallback to UUID if all combinations are taken (very unlikely)
    return QString("%1-copy-%2").arg(source_name, QUuid::createUuid().toString(QUuid::WithoutBraces).left(8));
}

std::string get_copy_disk_name(const std::string& source_name, const std::string& custom_name, const std::function<bool(const QString&)>& name_exists_check)
{
    if (!custom_name.empty())
    {
        return custom_name;
    }
    return generate_copy_disk_name(QString::fromStdString(source_name), name_exists_check).toStdString();
}
} // namespace

mp::ReturnCode cmd::CopyDisk::run(mp::ArgParser* parser)
{
    if (auto ret = parse_args(parser); ret != ParseCode::Ok)
    {
        return parser->returnCodeFrom(ret);
    }

    // First, list blocks to verify the source disk exists and get its path
    auto on_list_success = [this](mp::ListBlocksReply& reply) {
        // Note: ListBlocksReply doesn't have error_message field, errors are handled by grpc status

        // Find the source disk in the list
        std::string source_disk_path;
        std::string attached_instance;
        bool found = false;
        
        for (const auto& block : reply.block_devices())
        {
            if (block.name() == source_disk_name)
            {
                source_disk_path = block.path();
                found = true;
                if (!block.attached_to().empty())
                {
                    attached_instance = block.attached_to();
                }
                break;
            }
        }

        if (!found)
        {
            throw mp::ValidationException{
                fmt::format("Block device '{}' not found", source_disk_name)};
        }

        // If the disk is attached to a VM, check if the VM is running
        if (!attached_instance.empty())
        {
            InfoRequest info_request;
            info_request.add_instance_snapshot_pairs()->set_instance_name(attached_instance);

            auto on_info_success = [this, attached_instance, source_disk_path, reply](mp::InfoReply& info_reply) {
                if (!info_reply.details().empty())
                {
                    const auto& instance_info = info_reply.details(0);
                    if (instance_info.instance_status().status() == InstanceStatus::RUNNING ||
                        instance_info.instance_status().status() == InstanceStatus::STARTING ||
                        instance_info.instance_status().status() == InstanceStatus::RESTARTING)
                    {
                        throw mp::ValidationException{
                            fmt::format("Cannot copy block device '{}': it is attached to running VM '{}'. "
                                       "Stop the VM first before copying the disk.", 
                                       source_disk_name, attached_instance)};
                    }
                }

                // VM is stopped, proceed with copy
                return proceed_with_copy(source_disk_path, reply);
            };

            auto on_info_failure = [](grpc::Status& status) {
                throw mp::ValidationException{
                    fmt::format("Failed to get VM status: {}", status.error_message())};
                return ReturnCode::CommandFail;
            };

            return dispatch(&RpcMethod::info, info_request, on_info_success, on_info_failure);
        }

        // Disk is not attached, proceed with copy
        return proceed_with_copy(source_disk_path, reply);
    };

    auto on_list_failure = [](grpc::Status& status) {
        throw mp::ValidationException{
            fmt::format("Failed to connect to daemon: {}", status.error_message())};
        return ReturnCode::CommandFail;
    };

    return dispatch(&RpcMethod::list_blocks, list_request, on_list_success, on_list_failure);
}

mp::ReturnCode cmd::CopyDisk::proceed_with_copy(const std::string& source_disk_path, const mp::ListBlocksReply& reply)
{
    // Create a lambda to check if a disk name exists
    auto name_exists_check = [&reply](const QString& name) {
        for (const auto& block : reply.block_devices())
        {
            if (block.name() == name.toStdString())
            {
                return true;
            }
        }
        return false;
    };

    // Determine the name for the copy
    std::string copy_name = get_copy_disk_name(source_disk_name, custom_disk_name, name_exists_check);

    // Set up the create request to copy from the source disk
    create_request.set_name(copy_name);
    create_request.set_source_path(source_disk_path);

    // Now create the copy
    auto on_create_success = [this](mp::CreateBlockReply& create_reply) {
        if (!create_reply.error_message().empty())
        {
            throw mp::ValidationException{
                fmt::format("Failed to copy block device: {}", create_reply.error_message())};
        }
        // Server already logs the creation, so we don't need to print it again here
        return ReturnCode::Ok;
    };

    auto on_create_failure = [](grpc::Status& status) {
        throw mp::ValidationException{
            fmt::format("Failed to connect to daemon: {}", status.error_message())};
        return ReturnCode::CommandFail;
    };

    return dispatch(&RpcMethod::create_block, create_request, on_create_success, on_create_failure);
}

std::string cmd::CopyDisk::name() const
{
    return "copy-disk";
}

QString cmd::CopyDisk::short_help() const
{
    return QStringLiteral("Copy a block device");
}

QString cmd::CopyDisk::description() const
{
    return QStringLiteral("Copy an existing block device to create a new one.\n"
                          "The source block device must not be attached to a running VM.\n"
                          "If attached to a stopped VM, the copy will proceed.\n"
                          "Use --name to specify a custom name for the copy.");
}

mp::ParseCode cmd::CopyDisk::parse_args(mp::ArgParser* parser)
{
    parser->addPositionalArgument("source", "Name of the source block device to copy", "source");

    QCommandLineOption nameOption(QStringList() << "n" << "name",
                                  "Name for the copied block device. If not specified, "
                                  "a name will be auto-generated in the format '<source>-copy-<xx>'.",
                                  "name");
    parser->addOption(nameOption);

    auto status = parser->commandParse(this);

    if (status != ParseCode::Ok)
    {
        return status;
    }

    if (parser->positionalArguments().count() != 1)
    {
        throw mp::ValidationException{fmt::format(
            "Wrong number of arguments given. Expected 1 (<source block device name>)")};
    }

    const auto args = parser->positionalArguments();
    source_disk_name = args.at(0).toStdString();

    if (parser->isSet(nameOption))
    {
        custom_disk_name = parser->value(nameOption).toStdString();
    }

    return status;
}
