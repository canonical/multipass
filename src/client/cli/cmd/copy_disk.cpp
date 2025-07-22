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
        bool found = false;
        
        for (const auto& block : reply.block_devices())
        {
            if (block.name() == source_disk_name)
            {
                source_disk_path = block.path();
                found = true;
                break;
            }
        }

        if (!found)
        {
            throw mp::ValidationException{
                fmt::format("Block device '{}' not found", source_disk_name)};
        }

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
    };

    auto on_list_failure = [](grpc::Status& status) {
        throw mp::ValidationException{
            fmt::format("Failed to connect to daemon: {}", status.error_message())};
        return ReturnCode::CommandFail;
    };

    return dispatch(&RpcMethod::list_blocks, list_request, on_list_success, on_list_failure);
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
    return QStringLiteral("Copy an existing block device to create a new one.\n\n"
                          "Usage: multipass copy-disk <source-disk-name> [--name <copy-name>]\n\n"
                          "If --name is not specified, the copy will be named '{source-name}-copy-{xx}'\n"
                          "where 'xx' is a two-character alphanumeric identifier.");
}

mp::ParseCode cmd::CopyDisk::parse_args(mp::ArgParser* parser)
{
    parser->addPositionalArgument("source-disk-name", "Name of the block device to copy", "<source-disk-name>");

    parser->addOption({"name", "Name for the copied disk", "name"});

    auto status = parser->commandParse(this);
    if (status != ParseCode::Ok)
    {
        return status;
    }

    auto args = parser->positionalArguments();
    if (args.size() != 1)
    {
        cerr << "Wrong number of arguments\n";
        return ParseCode::CommandLineError;
    }

    source_disk_name = args[0].toStdString();

    if (parser->isSet("name"))
    {
        custom_disk_name = parser->value("name").toStdString();
        
        // Validate custom name
        if (custom_disk_name.empty())
        {
            cerr << "Custom disk name cannot be empty\n";
            return ParseCode::CommandLineError;
        }
        
        // Check for invalid characters (basic validation)
        for (char c : custom_disk_name)
        {
            if (!std::isalnum(c) && c != '-' && c != '_')
            {
                cerr << "Invalid character in disk name. Only alphanumeric characters, hyphens, and underscores are allowed\n";
                return ParseCode::CommandLineError;
            }
        }
    }

    return ParseCode::Ok;
}
