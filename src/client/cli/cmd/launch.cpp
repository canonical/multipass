/*
 * Copyright (C) 2017-2018 Canonical, Ltd.
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

#include "launch.h"
#include "common_cli.h"

#include "animated_spinner.h"
#include <multipass/cli/argparser.h>
#include <multipass/cli/client_platform.h>

#include <fmt/format.h>

#include <yaml-cpp/yaml.h>

#include <QTimeZone>

#include <regex>
#include <unordered_map>

namespace mp = multipass;
namespace cmd = multipass::cmd;
namespace mcp = multipass::cli::platform;
using RpcMethod = mp::Rpc::Stub;

namespace
{
const std::regex yes{"y|yes", std::regex::icase | std::regex::optimize};
const std::regex no{"n|no", std::regex::icase | std::regex::optimize};
const std::regex later{"l|later", std::regex::icase | std::regex::optimize};
const std::regex show{"s|show", std::regex::icase | std::regex::optimize};
} // namespace

mp::ReturnCode cmd::Launch::run(mp::ArgParser* parser)
{
    auto ret = parse_args(parser);
    if (ret != ParseCode::Ok)
    {
        return parser->returnCodeFrom(ret);
    }

    request.set_time_zone(QTimeZone::systemTimeZoneId().toStdString());

    return request_launch();
}

std::string cmd::Launch::name() const
{
    return "launch";
}

QString cmd::Launch::short_help() const
{
    return QStringLiteral("Create and start an Ubuntu instance");
}

QString cmd::Launch::description() const
{
    return QStringLiteral("Create and start a new instance.");
}

mp::ParseCode cmd::Launch::parse_args(mp::ArgParser* parser)
{
    parser->addPositionalArgument("image",
                                  "Optional image to launch. If omitted, then the default Ubuntu LTS "
                                  "will be used.\n"
                                  "<remote> can be either ‘release’ or ‘daily‘. If <remote> is omitted, "
                                  "‘release’ will be used.\n"
                                  "<image> can be a partial image hash or an Ubuntu release version, "
                                  "codename or alias.\n"
                                  "<url> is a custom image URL that is in http://, https://, or file:// "
                                  "format.\n",
                                  "[[<remote:>]<image> | <url>]");
    QCommandLineOption cpusOption({"c", "cpus"}, "Number of CPUs to allocate", "cpus", "1");
    QCommandLineOption diskOption({"d", "disk"}, "Disk space to allocate in bytes, or with K, M, G suffix", "disk",
                                  "default");
    QCommandLineOption memOption({"m", "mem"}, "Amount of memory to allocate in bytes, or with K, M, G suffix", "mem",
                                 "1024"); // In MB's
    QCommandLineOption nameOption({"n", "name"}, "Name for the instance", "name");
    QCommandLineOption cloudInitOption("cloud-init", "Path to a user-data cloud-init configuration", "file");
    parser->addOptions({cpusOption, diskOption, memOption, nameOption, cloudInitOption});

    auto status = parser->commandParse(this);

    if (status != ParseCode::Ok)
    {
        return status;
    }

    if (parser->positionalArguments().count() > 1)
    {
        cerr << "Too many arguments supplied\n";
        return ParseCode::CommandLineError;
    }

    if (!parser->positionalArguments().isEmpty())
    {
        auto remote_image_name = parser->positionalArguments().first();

        if (remote_image_name.startsWith("http://") || remote_image_name.startsWith("https://") ||
            remote_image_name.startsWith("file://"))
        {
            request.set_image(remote_image_name.toStdString());
        }
        else
        {
            auto colon_count = remote_image_name.count(':');

            if (colon_count > 1)
            {
                cerr << "Invalid remote and source image name supplied\n";
                return ParseCode::CommandLineError;
            }
            else if (colon_count == 1)
            {
                request.set_remote_name(remote_image_name.section(':', 0, 0).toStdString());
                request.set_image(remote_image_name.section(':', 1).toStdString());
            }
            else
            {
                request.set_image(remote_image_name.toStdString());
            }
        }
    }

    if (parser->isSet(nameOption))
    {
        request.set_instance_name(parser->value(nameOption).toStdString());
    }

    if (parser->isSet(cpusOption))
    {
        request.set_num_cores(parser->value(cpusOption).toInt());
    }

    if (parser->isSet(memOption))
    {
        request.set_mem_size(parser->value(memOption).toStdString());
    }

    if (parser->isSet(diskOption))
    {
        request.set_disk_space(parser->value(diskOption).toStdString());
    }

    if (parser->isSet(cloudInitOption))
    {
        try
        {
            auto node = YAML::LoadFile(parser->value(cloudInitOption).toStdString());
            request.set_cloud_init_user_data(YAML::Dump(node));
        }
        catch (const std::exception& e)
        {
            cerr << "error loading cloud-init config: " << e.what() << "\n";
            return ParseCode::CommandLineError;
        }
    }

    request.set_verbosity_level(parser->verbosityLevel());

    return status;
}

mp::ReturnCode cmd::Launch::request_launch()
{
    mp::AnimatedSpinner spinner{cout};

    auto on_success = [this, &spinner](mp::LaunchReply& reply) {
        spinner.stop();

        if (reply.metrics_pending())
        {
            if (mcp::is_tty())
            {
                cout << "One quick question before we launch … Would you like to help\n"
                     << "the Multipass developers, by sending anonymous usage data?\n"
                     << "This includes your operating system, which images you use,\n"
                     << "the number of instances, their properties and how long you use them.\n"
                     << "We’d also like to measure Multipass’s speed.\n\n"
                     << (reply.metrics_show_info().has_host_info() ? "Choose “show” to see an example usage report.\n\n"
                                                                     "Send usage data (yes/no/Later/show)? "
                                                                   : "Send usage data (yes/no/Later)? ");

                while (true)
                {
                    std::string answer;
                    std::getline(std::cin, answer);
                    if (std::regex_match(answer, yes))
                    {
                        request.mutable_opt_in_reply()->set_opt_in_status(OptInStatus::ACCEPTED);
                        cout << "Thank you!\n";
                        break;
                    }
                    else if (std::regex_match(answer, no))
                    {
                        request.mutable_opt_in_reply()->set_opt_in_status(OptInStatus::DENIED);
                        break;
                    }
                    else if (answer.empty() || std::regex_match(answer, later))
                    {
                        request.mutable_opt_in_reply()->set_opt_in_status(OptInStatus::LATER);
                        break;
                    }
                    else if (reply.metrics_show_info().has_host_info() && std::regex_match(answer, show))
                    {
                        // TODO: Display actual metrics data here provided by daemon
                        cout << "Show metrics example here\n\n"
                             << "Send usage data (yes/no/Later/show)? ";
                    }
                    else
                    {
                        cout << (reply.metrics_show_info().has_host_info() ? "Please answer yes/no/Later/show: "
                                                                           : "Please answer yes/no/Later: ");
                    }
                }
            }
            return request_launch();
        }

        cout << "Launched: " << reply.vm_instance_name() << "\n";
        return ReturnCode::Ok;
    };

    auto on_failure = [this, &spinner](grpc::Status& status) {
        spinner.stop();

        LaunchError launch_error;
        launch_error.ParseFromString(status.error_details());
        std::string error_details;

        for (const auto& error : launch_error.error_codes())
        {
            if (error == LaunchError::INVALID_DISK_SIZE)
            {
                error_details = fmt::format("Invalid disk size value supplied: {}", request.disk_space());
            }
            else if (error == LaunchError::INVALID_MEM_SIZE)
            {
                error_details = fmt::format("Invalid memory size value supplied: {}", request.mem_size());
            }
            else if (error == LaunchError::INVALID_HOSTNAME)
            {
                error_details = fmt::format("Invalid instance name supplied: {}", request.instance_name());
            }
        }

        return standard_failure_handler_for(name(), cerr, status, error_details);
    };

    auto streaming_callback = [this, &spinner](mp::LaunchReply& reply) {
        std::unordered_map<int, std::string> progress_messages{
            {LaunchProgress_ProgressTypes_IMAGE, "Retrieving image: "},
            {LaunchProgress_ProgressTypes_KERNEL, "Retrieving kernel image: "},
            {LaunchProgress_ProgressTypes_INITRD, "Retrieving initrd image: "},
            {LaunchProgress_ProgressTypes_EXTRACT, "Extracting image: "},
            {LaunchProgress_ProgressTypes_VERIFY, "Verifying image: "}};

        if (reply.create_oneof_case() == mp::LaunchReply::CreateOneofCase::kLaunchProgress)
        {
            auto& progress_message = progress_messages[reply.launch_progress().type()];
            if (reply.launch_progress().percent_complete() != "-1")
            {
                spinner.stop();
                cout << "\r";
                cout << progress_message << reply.launch_progress().percent_complete() << "%" << std::flush;
            }
            else
            {
                spinner.start(progress_message);
            }
        }
        else if (reply.create_oneof_case() == mp::LaunchReply::CreateOneofCase::kCreateMessage)
        {
            spinner.stop();
            spinner.start(reply.create_message());
        }
    };

    return dispatch(&RpcMethod::launch, request, on_success, on_failure, streaming_callback);
}
