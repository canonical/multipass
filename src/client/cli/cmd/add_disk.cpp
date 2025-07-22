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
    const QString letters = "abcdefhijkmnpqrstuvwxyz"; // no g to avoid disk-8g
    const QString alphanumeric = "abcdefghijkmnopqrstuvwxyz0123456789";

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

std::string get_disk_name(const std::string& custom_name, const std::function<bool(const QString&)>& name_exists_check)
{
    if (!custom_name.empty())
    {
        return custom_name;
    }
    return generate_unique_disk_name(name_exists_check).toStdString();
}
} // namespace

mp::ReturnCode cmd::AddDisk::run(mp::ArgParser* parser)
{
    if (auto ret = parse_args(parser); ret != ParseCode::Ok)
    {
        return parser->returnCodeFrom(ret);
    }

    // If no instance name is provided, just create the block device without attaching
    if (vm_name.empty())
    {
        auto on_create_success = [this](mp::CreateBlockReply& reply) {
            if (!reply.error_message().empty())
            {
                throw mp::ValidationException{
                    fmt::format("Failed to create block device: {}", reply.error_message())};
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

    // Both size and file path cases use create_block, then attach
    auto on_create_success = [this](mp::CreateBlockReply& reply) {
        if (!reply.error_message().empty())
        {
            throw mp::ValidationException{
                fmt::format("Failed to create block device: {}", reply.error_message())};
        }

        // Always extract the actual block device name from the server response
        // The server will either use our custom name or generate one if there's a conflict
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
            // Check if this is an instance not found error and we have a fallback option
            if (!single_arg_fallback.empty() &&
                (status.error_message().find("does not exist") != std::string::npos ||
                 status.error_message().find("not found") != std::string::npos)) {
                
                // Clean up the created block device since we're going to retry
                mp::DeleteBlockRequest delete_request;
                delete_request.set_name(actual_block_name);

                auto on_delete_success = [](mp::DeleteBlockReply& delete_reply) {
                    return ReturnCode::Ok;
                };

                auto on_delete_failure = [](grpc::Status& delete_status) {
                    // Ignore cleanup failures for fallback case
                    return ReturnCode::Ok;
                };

                // Clean up the block device (fire and forget)
                dispatch(&RpcMethod::delete_block,
                         delete_request,
                         on_delete_success,
                         on_delete_failure);

                // Retry as standalone disk creation with the original argument as size
                try {
                    // Validate that the fallback argument is a valid size
                    auto size = mp::MemorySize{single_arg_fallback};
                    if (size < mp::MemorySize{"1M"}) {
                        throw std::invalid_argument("Size too small");
                    }
                    
                    // Create new request for standalone disk
                    mp::CreateBlockRequest fallback_request;
                    auto name_exists_check = [](const QString& name) {
                        return false; // Server will handle collision detection
                    };
                    fallback_request.set_name(get_disk_name(custom_disk_name, name_exists_check));
                    fallback_request.set_size(single_arg_fallback);
                    // Leave instance_name empty for standalone creation
                    
                    auto fallback_success = [this](mp::CreateBlockReply& reply) {
                        if (!reply.error_message().empty()) {
                            throw mp::ValidationException{
                                fmt::format("Failed to create block device: {}", reply.error_message())};
                        }
                        return ReturnCode::Ok;
                    };

                    auto fallback_failure = [](grpc::Status& fallback_status) {
                        throw mp::ValidationException{
                            fmt::format("Failed to connect to daemon: {}", fallback_status.error_message())};
                        return ReturnCode::CommandFail;
                    };

                    return dispatch(&RpcMethod::create_block, fallback_request, fallback_success, fallback_failure);
                }
                catch (const std::invalid_argument&) {
                    // The fallback argument is not a valid size either
                    throw mp::ValidationException{
                        fmt::format("Instance '{}' does not exist and '{}' is not a valid disk size",
                                    vm_name, single_arg_fallback)};
                }
            }

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

            // Attempt to clean up the block device (fire and forget)
            dispatch(&RpcMethod::delete_block,
                     delete_request,
                     on_delete_success,
                     on_delete_failure);

            throw mp::ValidationException{
                fmt::format("Failed to attach block device: {}", status.error_message())};
            return ReturnCode::CommandFail;
        };

        return dispatch(&RpcMethod::attach_block,
                        attach_request,
                        on_attach_success,
                        on_attach_failure);
    };

    auto on_create_failure = [this](grpc::Status& status) {
        // Check if this is an instance not found error and we have a fallback option
        if (!single_arg_fallback.empty() &&
            (status.error_message().find("does not exist") != std::string::npos ||
             status.error_message().find("not found") != std::string::npos)) {
            
            // Retry as standalone disk creation with the original argument as size
            try {
                // Validate that the fallback argument is a valid size
                auto size = mp::MemorySize{single_arg_fallback};
                if (size < mp::MemorySize{"1M"}) {
                    throw std::invalid_argument("Size too small");
                }
                
                // Create new request for standalone disk
                mp::CreateBlockRequest fallback_request;
                auto name_exists_check = [](const QString& name) {
                    return false; // Server will handle collision detection
                };
                fallback_request.set_name(get_disk_name(custom_disk_name, name_exists_check));
                fallback_request.set_size(single_arg_fallback);
                // Leave instance_name empty for standalone creation
                
                auto fallback_success = [this](mp::CreateBlockReply& reply) {
                    if (!reply.error_message().empty()) {
                        throw mp::ValidationException{
                            fmt::format("Failed to create block device: {}", reply.error_message())};
                    }
                    return ReturnCode::Ok;
                };

                auto fallback_failure = [](grpc::Status& fallback_status) {
                    throw mp::ValidationException{
                        fmt::format("Failed to connect to daemon: {}", fallback_status.error_message())};
                    return ReturnCode::CommandFail;
                };

                return dispatch(&RpcMethod::create_block, fallback_request, fallback_success, fallback_failure);
            }
            catch (const std::invalid_argument&) {
                // The fallback argument is not a valid size either
                throw mp::ValidationException{
                    fmt::format("Instance '{}' does not exist and '{}' is not a valid disk size",
                                vm_name, single_arg_fallback)};
            }
        }

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
    return QStringLiteral("Add a disk to a VM instance or create a standalone disk");
}

QString cmd::AddDisk::description() const
{
    return QStringLiteral("Add a disk to a VM instance or create a standalone disk. You can either\n"
                          "specify a size to create a new disk (e.g., '10G'), or provide a path to\n"
                          "an existing disk image file. Supported formats for QEMU: qcow2, raw, vmdk,\n"
                          "vdi, vhd, vpc.\n\n"
                          "Usage:\n"
                          "  multipass add-disk                    # Create standalone 10G disk\n"
                          "  multipass add-disk 5G                 # Create standalone 5G disk\n"
                          "  multipass add-disk 5G --name cool-disk # Create 5G disk named 'cool-disk'\n"
                          "  multipass add-disk myvm               # Add 10G disk to 'myvm'\n"
                          "  multipass add-disk 5G myvm            # Add 5G disk to 'myvm'\n"
                          "  multipass add-disk myvm 5G            # Add 5G disk to 'myvm'\n\n"
                          "Options:\n"
                          "  --name <name>  Custom name for the disk (e.g., 'cool-disk')\n\n"
                          "If no instance is specified, a standalone disk will be created that can\n"
                          "later be attached to any VM using the attach-block command.\n"
                          "When attaching to a VM, the VM must be in a stopped state.");
}

mp::ParseCode cmd::AddDisk::parse_args(mp::ArgParser* parser)
{
    parser->addPositionalArgument("arg1",
                                  "Disk size (e.g., '10G'), disk image file path, or VM instance name",
                                  "[arg1]");

    parser->addPositionalArgument(
        "arg2",
        "VM instance name (if arg1 is disk size/path) or disk size/path (if arg1 is instance name)",
        "[arg2]");

    parser->addOption({"name", "Custom name for the disk (e.g., 'cool-disk')", "name"});

    auto status = parser->commandParse(this);

    if (status != ParseCode::Ok)
    {
        return status;
    }

    // Capture custom disk name if provided
    if (parser->isSet("name"))
    {
        custom_disk_name = parser->value("name").toStdString();
        
        // Validate custom name (basic validation)
        if (custom_disk_name.empty())
        {
            throw mp::ValidationException{"Custom disk name cannot be empty"};
        }
        
        // Check for invalid characters (basic validation)
        if (custom_disk_name.find('/') != std::string::npos ||
            custom_disk_name.find('\\') != std::string::npos)
        {
            throw mp::ValidationException{"Custom disk name cannot contain path separators"};
        }
    }

    const auto args = parser->positionalArguments();
    
    if (args.size() == 0)
    {
        // No arguments: create standalone disk with default size
        disk_input = default_disk_size;
        vm_name = "";
    }
    else if (args.size() == 1)
    {
        QString arg1 = args.at(0);
        
        // For single argument, always try as instance name first (daemon will validate)
        // If daemon says instance doesn't exist, we'll fall back to treating as size
        vm_name = arg1.toStdString();
        disk_input = default_disk_size;
        
        // Store the original argument for potential fallback
        single_arg_fallback = arg1.toStdString();
    }
    else if (args.size() == 2)
    {
        QString arg1 = args.at(0);
        QString arg2 = args.at(1);
        
        if (is_size_string(arg1))
        {
            // First arg is size, second is VM name: add specified size disk to VM
            disk_input = arg1.toStdString();
            vm_name = arg2.toStdString();
        }
        else
        {
            // First arg is VM name, second is size: add specified size disk to VM
            vm_name = arg1.toStdString();
            disk_input = arg2.toStdString();
        }
    }
    else
    {
        throw mp::ValidationException{"add-disk accepts at most 2 arguments"};
    }

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
        create_request.set_name(get_disk_name(custom_disk_name, name_exists_check));
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
        create_request.set_name(get_disk_name(custom_disk_name, name_exists_check));
        create_request.set_source_path(file_info.absoluteFilePath().toStdString());
        create_request.set_size(""); // Explicitly set empty size for file path case
        create_request.set_instance_name(vm_name);
    }

    return status;
}
