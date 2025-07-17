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

#include "add_disk.h"
#include "common_cli.h"

#include <multipass/cli/argparser.h>
#include <multipass/exceptions/cmd_exceptions.h>
#include <multipass/format.h>
#include <multipass/memory_size.h>

#include <QDir>
#include <QFileInfo>
#include <QRandomGenerator>
#include <QUuid>

namespace mp = multipass;
namespace cmd = multipass::cmd;

namespace
{
constexpr auto default_disk_size = "10G";
constexpr auto min_disk_size = "1G";

// QEMU supported disk image formats
const QStringList supported_formats = {"qcow2", "raw", "vmdk", "vdi", "vhd", "vpc"};

bool is_size_string(const QString& input)
{
    // Check if the input looks like a size (ends with K, M, G, or is just a number)
    if (input.isEmpty())
        return false;

    QChar last_char = input.back().toUpper();
    if (last_char == 'K' || last_char == 'M' || last_char == 'G')
        return true;

    // Check if it's just a number (would be interpreted as bytes)
    bool ok;
    input.toLongLong(&ok);
    return ok;
}

bool is_supported_disk_format(const QString& file_path)
{
    QFileInfo file_info(file_path);
    QString extension = file_info.suffix().toLower();
    return supported_formats.contains(extension);
}

QString generate_unique_disk_name(const std::function<bool(const QString&)>& name_exists_check)
{
    // Generate 2-character alphanumeric names with at least one letter
    const QString letters = "abcdefhijklmnopqrstuvwxyz"; // no g to avoid disk-8g
    const QString alphanumeric = "abcdefghijklmnopqrstuvwxyz0123456789";

    for (int attempts = 0; attempts < 1000; ++attempts)
    {
        QString disk_id;

        // Ensure at least one character is a letter
        if (QRandomGenerator::global()->bounded(2) == 0)
        {
            // First char is letter, second can be anything
            disk_id += letters[QRandomGenerator::global()->bounded(letters.length())];
            disk_id += alphanumeric[QRandomGenerator::global()->bounded(alphanumeric.length())];
        }
        else
        {
            // Second char is letter, first can be anything
            disk_id += alphanumeric[QRandomGenerator::global()->bounded(alphanumeric.length())];
            disk_id += letters[QRandomGenerator::global()->bounded(letters.length())];
        }

        QString disk_name = QString("disk-%1").arg(disk_id);

        if (!name_exists_check(disk_name))
        {
            return disk_name;
        }
    }

    // Fallback to UUID if all combinations are taken (very unlikely)
    return QString("disk-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces).left(8));
}
} // namespace

mp::ReturnCode cmd::AddDisk::run(mp::ArgParser* parser)
{
    if (auto ret = parse_args(parser); ret != ParseCode::Ok)
    {
        return parser->returnCodeFrom(ret);
    }

    // Both size and file path cases use create_block, then attach
    auto on_create_success = [this](mp::CreateBlockReply& reply) {
        if (!reply.error_message().empty())
        {
            throw mp::ValidationException{
                fmt::format("Failed to create block device: {}", reply.error_message())};
        }

        // Extract the actual block device name from the server response
        std::string actual_block_name = create_request.name();
        if (!reply.log_line().empty())
        {
            // Parse the log line to extract the actual name: "Created block device 'disk-xx'"
            std::string log_line = reply.log_line();
            size_t start = log_line.find("'") + 1;
            size_t end = log_line.find("'", start);
            if (start != std::string::npos && end != std::string::npos)
            {
                actual_block_name = log_line.substr(start, end - start);
            }
        }

        // Server already logs the creation, so we don't need to print it again here

        // Now attach the created disk using the actual name
        attach_request.set_block_name(actual_block_name);
        attach_request.set_instance_name(vm_name);

        auto on_attach_success = [this, actual_block_name](mp::AttachBlockReply& attach_reply) {
            if (!attach_reply.error_message().empty())
            {
                // Clean up the created block device since attachment failed
                mp::DeleteBlockRequest delete_request;
                delete_request.set_name(actual_block_name);

                auto on_delete_success = [](mp::DeleteBlockReply& delete_reply) {
                    return ReturnCode::Ok;
                };

                auto on_delete_failure = [this](grpc::Status& delete_status) {
                    cerr << fmt::format("Warning: Failed to clean up created block device: {}\n",
                                        delete_status.error_message());
                    return ReturnCode::Ok;
                };

                // Attempt to clean up the block device
                dispatch(&RpcMethod::delete_block,
                         delete_request,
                         on_delete_success,
                         on_delete_failure);

                throw mp::ValidationException{
                    fmt::format("Failed to attach block device: {}", attach_reply.error_message())};
            }
            // Server already logs the attachment, so we don't need to print it again here
            return ReturnCode::Ok;
        };

        auto on_attach_failure = [this, actual_block_name](grpc::Status& status) {
            // Clean up the created block device since attachment failed
            mp::DeleteBlockRequest delete_request;
            delete_request.set_name(actual_block_name);

            auto on_delete_success = [](mp::DeleteBlockReply& delete_reply) {
                // Block device cleanup successful, no need to log
                return ReturnCode::Ok;
            };

            auto on_delete_failure = [this](grpc::Status& delete_status) {
                // Log warning but don't fail the command since the main error is attachment failure
                cerr << fmt::format("Warning: Failed to clean up created block device: {}\n",
                                    delete_status.error_message());
                return ReturnCode::Ok;
            };

            // Attempt to clean up the block device (fire and forget)
            dispatch(&RpcMethod::delete_block,
                     delete_request,
                     on_delete_success,
                     on_delete_failure);

            throw mp::ValidationException{
                fmt::format("Failed to connect to daemon: {}", status.error_message())};
            return ReturnCode::CommandFail;
        };

        return dispatch(&RpcMethod::attach_block,
                        attach_request,
                        on_attach_success,
                        on_attach_failure);
    };

    auto on_create_failure = [](grpc::Status& status) {
        throw mp::ValidationException{
            fmt::format("Failed to connect to daemon: {}", status.error_message())};
        return ReturnCode::CommandFail;
    };

    return dispatch(&RpcMethod::create_block, create_request, on_create_success, on_create_failure);
}

std::string cmd::AddDisk::name() const
{
    return "add-disk";
}

QString cmd::AddDisk::short_help() const
{
    return QStringLiteral("Add a disk to a VM instance");
}

QString cmd::AddDisk::description() const
{
    return QStringLiteral("Add a disk to a VM instance. You can either specify a size to create\n"
                          "a new disk (e.g., '10G'), or provide a path to an existing disk image\n"
                          "file. Supported formats for QEMU: qcow2, raw, vmdk, vdi, vhd, vpc.\n"
                          "The VM must be in a stopped state.");
}

mp::ParseCode cmd::AddDisk::parse_args(mp::ArgParser* parser)
{
    parser->addPositionalArgument("instance",
                                  "Name of the VM instance to add the disk to",
                                  "instance");

    parser->addPositionalArgument("disk",
                                  "Disk size (e.g., '10G') or path to existing disk image file",
                                  "disk");

    auto status = parser->commandParse(this);

    if (status != ParseCode::Ok)
    {
        return status;
    }

    if (parser->positionalArguments().count() != 2)
    {
        throw mp::ValidationException{"add-disk requires exactly 2 arguments: <instance> <disk>"};
    }

    const auto args = parser->positionalArguments();
    vm_name = args.at(0).toStdString();
    disk_input = args.at(1).toStdString();

    QString disk_qstring = QString::fromStdString(disk_input);

    // Determine if the input is a size or a file path
    if (is_size_string(disk_qstring))
    {
        is_size_input = true;

        // Validate the size
        try
        {
            auto size = mp::MemorySize{disk_input};
            if (size < mp::MemorySize{min_disk_size})
            {
                throw mp::ValidationException{
                    fmt::format("Disk size '{}' is too small, minimum size is {}",
                                disk_input,
                                min_disk_size)};
            }
        }
        catch (const std::invalid_argument&)
        {
            throw mp::ValidationException{fmt::format(
                "Invalid disk size '{}', must be a positive number with K, M, or G suffix",
                disk_input)};
        }

        // Set up create request with collision detection
        auto name_exists_check = [](const QString& name) {
            // For client-side, we can't check server state, so just return false
            // The server will handle actual collision detection
            return false;
        };
        create_request.set_name(generate_unique_disk_name(name_exists_check).toStdString());
        create_request.set_size(disk_input);
        create_request.set_instance_name(vm_name);
    }
    else
    {
        is_size_input = false;

        // Validate the file path
        QFileInfo file_info(disk_qstring);
        if (!file_info.exists())
        {
            throw mp::ValidationException{
                fmt::format("Disk image file '{}' does not exist", disk_input)};
        }

        if (!file_info.isFile())
        {
            throw mp::ValidationException{fmt::format("'{}' is not a regular file", disk_input)};
        }

        if (!is_supported_disk_format(disk_qstring))
        {
            throw mp::ValidationException{
                fmt::format("Unsupported disk format for '{}'. Supported formats: {}",
                            disk_input,
                            supported_formats.join(", ").toStdString())};
        }

        // Set up create request with the source file path
        auto name_exists_check = [](const QString& name) {
            // For client-side, we can't check server state, so just return false
            // The server will handle actual collision detection
            return false;
        };
        create_request.set_name(generate_unique_disk_name(name_exists_check).toStdString());
        create_request.set_source_path(file_info.absoluteFilePath().toStdString());
        create_request.set_size(""); // Explicitly set empty size for file path case
        create_request.set_instance_name(vm_name);
    }

    return status;
}
