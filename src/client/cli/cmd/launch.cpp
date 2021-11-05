/*
 * Copyright (C) 2017-2021 Canonical, Ltd.
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
#include "animated_spinner.h"
#include "common_cli.h"

#include <multipass/cli/argparser.h>
#include <multipass/cli/client_platform.h>
#include <multipass/constants.h>
#include <multipass/exceptions/cmd_exceptions.h>
#include <multipass/exceptions/snap_environment_exception.h>
#include <multipass/format.h>
#include <multipass/memory_size.h>
#include <multipass/settings/settings.h>
#include <multipass/snap_utils.h>
#include <multipass/utils.h>

#include <yaml-cpp/yaml.h>

#include <QDir>
#include <QFileInfo>
#include <QTimeZone>

#include <cstdlib>
#include <regex>
#include <unordered_map>

namespace mp = multipass;
namespace mpu = multipass::utils;
namespace cmd = multipass::cmd;
namespace mcp = multipass::cli::platform;
using RpcMethod = mp::Rpc::Stub;

namespace
{
const std::regex yes{"y|yes", std::regex::icase | std::regex::optimize};
const std::regex no{"n|no", std::regex::icase | std::regex::optimize};
const std::regex later{"l|later", std::regex::icase | std::regex::optimize};
const std::regex show{"s|show", std::regex::icase | std::regex::optimize};

constexpr bool on_windows()
{ // TODO when we have remote client-daemon communication, we need to get the daemon's platform
    return
#ifdef MULTIPASS_PLATFORM_WINDOWS
        true;
#else
        false;
#endif
}

auto checked_mode(const std::string& mode)
{
    if (mode == "auto")
        return mp::LaunchRequest_NetworkOptions_Mode::LaunchRequest_NetworkOptions_Mode_AUTO;
    if (mode == "manual")
        return mp::LaunchRequest_NetworkOptions_Mode::LaunchRequest_NetworkOptions_Mode_MANUAL;
    else
        throw mp::ValidationException{fmt::format("Bad network mode '{}', need 'auto' or 'manual'", mode)};
}

const std::string& checked_mac(const std::string& mac)
{
    if (!mpu::valid_mac_address(mac))
        throw mp::ValidationException(fmt::format("Invalid MAC address: {}", mac));

    return mac;
}

auto net_digest(const QString& options)
{
    multipass::LaunchRequest_NetworkOptions net;
    QStringList split = options.split(',', QString::SkipEmptyParts);
    for (const auto& key_value_pair : split)
    {
        QStringList key_value_split = key_value_pair.split('=', QString::SkipEmptyParts);
        if (key_value_split.size() == 2)
        {

            const auto& key = key_value_split[0].toLower();
            const auto& val = key_value_split[1];
            if (key == "name")
                net.set_id(val.toStdString());
            else if (key == "mode")
                net.set_mode(checked_mode(val.toLower().toStdString()));
            else if (key == "mac")
                net.set_mac_address(checked_mac(val.toStdString()));
            else
                throw mp::ValidationException{fmt::format("Bad network field: {}", key)};
        }

        // Interpret as "name" the argument when there are no ',' and no '='.
        else if (key_value_split.size() == 1 && split.size() == 1)
            net.set_id(key_value_split[0].toStdString());

        else
            throw mp::ValidationException{fmt::format("Bad network field definition: {}", key_value_pair)};
    }

    if (net.id().empty())
        throw mp::ValidationException{fmt::format("Bad network definition, need at least a 'name' field")};

    return net;
}
} // namespace

mp::ReturnCode cmd::Launch::run(mp::ArgParser* parser)
{
    petenv_name = MP_SETTINGS.get(petenv_key);
    if (auto ret = parse_args(parser); ret != ParseCode::Ok)
    {
        return parser->returnCodeFrom(ret);
    }

    auto ret = request_launch(parser);
    if (ret == ReturnCode::Ok && request.instance_name() == petenv_name.toStdString())
    {
        std::string mounts_value;
        if (std::tie(ret, mounts_value) = request_mounts_setting_from_daemon(parser); ret == ReturnCode::Ok)
        {
            if (mounts_value != "true")
                cout << fmt::format("Skipping '{}' mount due to disabled mounts feature\n", mp::home_automount_dir);
            else
                ret = mount_home(parser);
        }
    }

    return ret;
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
    QCommandLineOption cpusOption({"c", "cpus"},
                                  QString::fromStdString(fmt::format("Number of CPUs to allocate.\n"
                                                                     "Minimum: {}, default: {}.",
                                                                     min_cpu_cores, default_cpu_cores)),
                                  "cpus", default_cpu_cores);
    QCommandLineOption diskOption(
        {"d", "disk"},
        QString::fromStdString(fmt::format("Disk space to allocate. Positive integers, in "
                                           "bytes, or with K, M, G suffix.\nMinimum: {}, default: {}.",
                                           min_disk_size, default_disk_size)),
        "disk", QString::fromUtf8(default_disk_size));
    QCommandLineOption memOption(
        {"m", "mem"},
        QString::fromStdString(fmt::format("Amount of memory to allocate. Positive integers, "
                                           "in bytes, or with K, M, G suffix.\nMinimum: {}, default: {}.",
                                           min_memory_size, default_memory_size)),
        "mem", QString::fromUtf8(default_memory_size)); // In MB's

    const auto name_option_desc =
        petenv_name.isEmpty()
            ? QString{"Name for the instance."}
            : QString{"Name for the instance. If it is '%1' (the configured primary instance name), the user's home "
                      "directory is mounted inside the newly launched instance, in '%2'."}
                  .arg(petenv_name, mp::home_automount_dir);

    QCommandLineOption nameOption({"n", "name"}, name_option_desc, "name");
    QCommandLineOption cloudInitOption("cloud-init", "Path to a user-data cloud-init configuration, or '-' for stdin",
                                       "file");
    QCommandLineOption networkOption("network",
                                     "Add a network interface to the instance, where <spec> is in the "
                                     "\"key=value,key=value\" format, with the following keys available:\n"
                                     "  name: the network to connect to (required), use the networks command for a "
                                     "list of possible values, or use 'bridged' to use the interface configured via "
                                     "`multipass set local.bridged-network`.\n"
                                     "  mode: auto|manual (default: auto)\n"
                                     "  mac: hardware address (default: random).\n"
                                     "You can also use a shortcut of \"<name>\" to mean \"name=<name>\".",
                                     "spec");
    QCommandLineOption bridgedOption("bridged", "Adds one `--network bridged` network.");

    parser->addOptions({cpusOption, diskOption, memOption, nameOption, cloudInitOption, networkOption, bridgedOption});

    mp::cmd::add_timeout(parser);

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
        bool conversion_pass;
        const auto& cpu_text = parser->value(cpusOption);
        const int cpu_count = cpu_text.toInt(&conversion_pass); // Base 10 conversion with check.

        if (!conversion_pass || cpu_count < 1)
        {
            fmt::print(cerr, "error: Invalid CPU count '{}', need a positive integer value.\n", cpu_text);
            return ParseCode::CommandLineError;
        }

        request.set_num_cores(cpu_count);
    }

    if (parser->isSet(memOption))
    {
        auto arg_mem_size = parser->value(memOption).toStdString();

        mp::MemorySize{arg_mem_size}; // throw if bad

        request.set_mem_size(arg_mem_size);
    }

    if (parser->isSet(diskOption))
    {
        auto arg_disk_size = parser->value(diskOption).toStdString();

        mp::MemorySize{arg_disk_size}; // throw if bad

        request.set_disk_space(arg_disk_size);
    }

    if (parser->isSet(cloudInitOption))
    {
        try
        {
            YAML::Node node;
            const QString& cloudInitFile = parser->value(cloudInitOption);
            if (cloudInitFile == "-")
            {
                node = YAML::Load(term->read_all_cin());
            }
            else
            {
                QFileInfo check_file(cloudInitFile);

                if (!check_file.exists() || !check_file.isFile())
                {
                    cerr << "error: No such file: " << cloudInitFile.toStdString() << "\n";
                    return ParseCode::CommandLineError;
                }

                node = YAML::LoadFile(cloudInitFile.toStdString());
            }
            request.set_cloud_init_user_data(YAML::Dump(node));
        }
        catch (const std::exception& e)
        {
            cerr << "error loading cloud-init config: " << e.what() << "\n";
            return ParseCode::CommandLineError;
        }
    }

    if (parser->isSet(bridgedOption))
    {
        request.mutable_network_options()->Add(net_digest(mp::bridged_network_name));
    }

    try
    {
        if (parser->isSet(networkOption))
            for (const auto& net : parser->values(networkOption))
                request.mutable_network_options()->Add(net_digest(net));

        request.set_timeout(mp::cmd::parse_timeout(parser));
    }
    catch (mp::ValidationException& e)
    {
        cerr << "error: " << e.what() << "\n";
        return ParseCode::CommandLineError;
    }

    request.set_time_zone(QTimeZone::systemTimeZoneId().toStdString());
    request.set_verbosity_level(parser->verbosityLevel());

    return status;
}

mp::ReturnCode cmd::Launch::request_launch(const ArgParser* parser)
{
    if (!spinner)
        spinner = std::make_unique<multipass::AnimatedSpinner>(
            cout); // Creating just in time to work around canonical/multipass#2075

    if (timer)
        timer->resume();
    else if (parser->isSet("timeout"))
    {
        timer = cmd::make_timer(parser->value("timeout").toInt(), spinner.get(), cerr,
                                "Timed out waiting for instance launch.");
        timer->start();
    }

    auto on_success = [this, &parser](mp::LaunchReply& reply) {
        spinner->stop();
        if (timer)
            timer->pause();

        if (reply.metrics_pending())
        {
            request.mutable_opt_in_reply()->set_opt_in_status(ask_metrics_permission(reply));
            return request_launch(parser);
        }

        cout << "Launched: " << reply.vm_instance_name() << "\n";

        if (term->is_live() && update_available(reply.update_info()))
        {
            // TODO: daemon doesn't know if client actually shows this notice. Need to be able
            // to tell daemon that the notice will be displayed or not.
            cout << update_notice(reply.update_info());
        }

        return ReturnCode::Ok;
    };

    auto on_failure = [this, &parser](grpc::Status& status, mp::LaunchReply& reply) {
        spinner->stop();
        if (timer)
            timer->pause();

        LaunchError launch_error;
        launch_error.ParseFromString(status.error_details());
        std::string error_details;

        for (const auto& error : launch_error.error_codes())
        {
            if (error == LaunchError::INVALID_DISK_SIZE)
            {
                error_details = fmt::format("Invalid disk size value supplied: {}.", request.disk_space());
            }
            else if (error == LaunchError::INVALID_MEM_SIZE)
            {
                error_details = fmt::format("Invalid memory size value supplied: {}.", request.mem_size());
            }
            else if (error == LaunchError::INVALID_HOSTNAME)
            {
                error_details = fmt::format("Invalid instance name supplied: {}", request.instance_name());
            }
            else if (error == LaunchError::INVALID_NETWORK)
            {
                if (reply.nets_need_bridging_size() && ask_bridge_permission(reply))
                {
                    request.set_permission_to_bridge(true);
                    return request_launch(parser);
                }

                // TODO: show the option which triggered the error only. This will need a refactor in the
                // LaunchError proto.
                error_details = "Invalid network options supplied";
            }
        }

        return standard_failure_handler_for(name(), cerr, status, error_details);
    };

    auto streaming_callback = [this](mp::LaunchReply& reply) {
        std::unordered_map<int, std::string> progress_messages{
            {LaunchProgress_ProgressTypes_IMAGE, "Retrieving image: "},
            {LaunchProgress_ProgressTypes_KERNEL, "Retrieving kernel image: "},
            {LaunchProgress_ProgressTypes_INITRD, "Retrieving initrd image: "},
            {LaunchProgress_ProgressTypes_EXTRACT, "Extracting image: "},
            {LaunchProgress_ProgressTypes_VERIFY, "Verifying image: "},
            {LaunchProgress_ProgressTypes_WAITING, "Preparing image: "}};

        if (!reply.log_line().empty())
        {
            spinner->print(cerr, reply.log_line());
        }

        if (reply.create_oneof_case() == mp::LaunchReply::CreateOneofCase::kLaunchProgress)
        {
            auto& progress_message = progress_messages[reply.launch_progress().type()];
            if (reply.launch_progress().percent_complete() != "-1")
            {
                spinner->stop();
                cout << "\r";
                cout << progress_message << reply.launch_progress().percent_complete() << "%" << std::flush;
            }
            else
            {
                spinner->stop();
                spinner->start(progress_message);
            }
        }
        else if (reply.create_oneof_case() == mp::LaunchReply::CreateOneofCase::kCreateMessage)
        {
            spinner->stop();
            spinner->start(reply.create_message());
        }
        else if (!reply.reply_message().empty())
        {
            spinner->stop();
            spinner->start(reply.reply_message());
        }
    };

    return dispatch(&RpcMethod::launch, request, on_success, on_failure, streaming_callback);
}

auto cmd::Launch::mount_home(const mp::ArgParser* parser) -> ReturnCode
{
    QString mount_source{};
    try
    {
        mount_source = QString::fromLocal8Bit(mpu::snap_real_home_dir());
    }
    catch (const SnapEnvironmentException&)
    {
        mount_source = QDir::toNativeSeparators(QDir::homePath());
    }

    const auto mount_target = QString{"%1:%2"}.arg(petenv_name, home_automount_dir);

    auto ret = run_cmd({"multipass", "mount", mount_source, mount_target}, parser, cout, cerr);
    if (ret == Ok)
        cout << fmt::format("Mounted '{}' into '{}'\n", mount_source, mount_target);

    return ret;
}

auto cmd::Launch::request_mounts_setting_from_daemon(const ArgParser* parser) -> std::pair<ReturnCode, std::string>
{
    // TODO: This needs wrapping in mp::Settings
    std::string mounts_value = "false";
    mp::GetRequest get_request;

    auto on_success = [&mounts_value](mp::GetReply& reply) {
        mounts_value = reply.value();
        return ReturnCode::Ok;
    };

    auto on_failure = [this](grpc::Status& status) { return standard_failure_handler_for(name(), cerr, status); };

    get_request.set_key(mp::mounts_key);
    get_request.set_verbosity_level(parser->verbosityLevel());
    auto ret = dispatch(&RpcMethod::get, get_request, on_success, on_failure);

    return {ret, mounts_value};
}

auto cmd::Launch::ask_metrics_permission(const mp::LaunchReply& reply) -> OptInStatus::Status
{
    if (term->is_live())
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
            std::getline(term->cin(), answer);
            if (std::regex_match(answer, yes))
            {
                cout << "Thank you!\n";
                return OptInStatus::ACCEPTED;
            }
            else if (std::regex_match(answer, no))
            {
                return OptInStatus::DENIED;
            }
            else if (answer.empty() || std::regex_match(answer, later))
            {
                return OptInStatus::LATER;
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

    return OptInStatus::UNKNOWN;
}

bool cmd::Launch::ask_bridge_permission(multipass::LaunchReply& reply)
{
    static constexpr auto plural = "Multipass needs to create {} to connect to {}.\nThis will temporarily disrupt "
                                   "connectivity on those interfaces.\n\nDo you want to continue (yes/no)? ";
    static constexpr auto singular = "Multipass needs to create a {} to connect to {}.\nThis will temporarily disrupt "
                                     "connectivity on that interface.\n\nDo you want to continue (yes/no)? ";
    static constexpr auto nodes = on_windows() ? "switches" : "bridges";
    static constexpr auto node = on_windows() ? "switch" : "bridge";

    if (term->is_live())
    {
        assert(reply.nets_need_bridging_size()); // precondition
        if (reply.nets_need_bridging_size() != 1)
            fmt::print(cout, plural, nodes, fmt::join(reply.nets_need_bridging(), ", "));
        else
            fmt::print(cout, singular, node, reply.nets_need_bridging(0));

        while (true)
        {
            std::string answer;
            std::getline(term->cin(), answer);
            if (std::regex_match(answer, yes))
                return true;
            else if (std::regex_match(answer, no))
                return false;
            else
                cout << "Please answer yes/no: ";
        }
    }

    return false;
}
