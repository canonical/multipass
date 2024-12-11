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

#include "launch.h"
#include "animated_spinner.h"
#include "common_cli.h"
#include "create_alias.h"

#include <multipass/cli/argparser.h>
#include <multipass/cli/client_platform.h>
#include <multipass/cli/prompters.h>
#include <multipass/constants.h>
#include <multipass/exceptions/cmd_exceptions.h>
#include <multipass/exceptions/snap_environment_exception.h>
#include <multipass/file_ops.h>
#include <multipass/format.h>
#include <multipass/memory_size.h>
#include <multipass/settings/settings.h>
#include <multipass/snap_utils.h>
#include <multipass/standard_paths.h>
#include <multipass/url_downloader.h>
#include <multipass/utils.h>

#include <yaml-cpp/yaml.h>

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTimeZone>
#include <QUrl>
#include <filesystem>
#include <unordered_map>

namespace mp = multipass;
namespace mpu = multipass::utils;
namespace cmd = multipass::cmd;
namespace mcp = multipass::cli::platform;
namespace fs = std::filesystem;

namespace
{
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
    QStringList split = options.split(',', Qt::SkipEmptyParts);
    for (const auto& key_value_pair : split)
    {
        QStringList key_value_split = key_value_pair.split('=', Qt::SkipEmptyParts);
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

    if (ret != ReturnCode::Ok)
        return ret;

    auto got_petenv = instance_name == petenv_name;
    if (!got_petenv && mount_routes.empty())
        return ret;

    if (MP_SETTINGS.get_as<bool>(mounts_key))
    {
        auto has_home_mount = std::count_if(mount_routes.begin(), mount_routes.end(),
                                            [](const auto& route) { return route.second == home_automount_dir; });

        if (got_petenv && !has_home_mount)
        {
            try
            {
                mount_routes.emplace_back(QString::fromLocal8Bit(mpu::snap_real_home_dir()), home_automount_dir);
            }
            catch (const SnapEnvironmentException&)
            {
                mount_routes.emplace_back(QDir::toNativeSeparators(QDir::homePath()), home_automount_dir);
            }
        }

        for (const auto& [source, target] : mount_routes)
        {
            auto mount_ret = mount(parser, source, target);
            if (ret == ReturnCode::Ok)
            {
                ret = mount_ret;
            }
        }
    }
    else
    {
        cout << "Skipping mount due to disabled mounts feature\n";
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
                                           "bytes, or decimals, with K, M, G suffix.\nMinimum: {}, default: {}.",
                                           min_disk_size, default_disk_size)),
        "disk", QString::fromUtf8(default_disk_size));

    QCommandLineOption memOption(
        {"m", "memory"},
        QString::fromStdString(fmt::format("Amount of memory to allocate. Positive integers, "
                                           "in bytes, or decimals, with K, M, G suffix.\nMinimum: {}, default: {}.",
                                           min_memory_size, default_memory_size)),
        "memory", QString::fromUtf8(default_memory_size)); // In MB's
    QCommandLineOption memOptionDeprecated(
        "mem", QString::fromStdString("Deprecated memory allocation long option. See \"--memory\"."), "memory",
        QString::fromUtf8(default_memory_size));
    memOptionDeprecated.setFlags(QCommandLineOption::HiddenFromHelp);

    const auto valid_name_desc = QString{"Valid names must consist of letters, numbers, or hyphens, must start with a "
                                         "letter, and must end with an alphanumeric character."};
    const auto name_option_desc =
        petenv_name.isEmpty()
            ? QString{"Name for the instance.\n%1"}.arg(valid_name_desc)
            : QString{"Name for the instance. If it is '%1' (the configured primary instance name), the user's home "
                      "directory is mounted inside the newly launched instance, in '%2'.\n%3"}
                  .arg(petenv_name, mp::home_automount_dir, valid_name_desc);

    QCommandLineOption nameOption({"n", "name"}, name_option_desc, "name");
    QCommandLineOption cloudInitOption("cloud-init",
                                       "Path or URL to a user-data cloud-init configuration, or '-' for stdin.",
                                       "file> | <url");
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
    QCommandLineOption mountOption("mount",
                                   "Mount a local directory inside the instance. If <target> is omitted, the "
                                   "mount point will be under /home/ubuntu/<source-dir>, where <source-dir> is "
                                   "the name of the <source> directory.",
                                   "source>:<target");

    parser->addOptions({cpusOption, diskOption, memOption, memOptionDeprecated, nameOption, cloudInitOption,
                        networkOption, bridgedOption, mountOption});

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

        if (remote_image_name.startsWith("file://"))
        {
            // Convert to absolute because the daemon doesn't know where the client is being executed.
            auto file_info = QFileInfo(remote_image_name.remove(0, 7));
            auto absolute_file_path = file_info.absoluteFilePath();

            request.set_image("file://" + absolute_file_path.toStdString());
        }
        else if (remote_image_name.startsWith("http://") || remote_image_name.startsWith("https://"))
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
            fmt::print(cerr, "Error: invalid CPU count '{}', need a positive integer value.\n", cpu_text);
            return ParseCode::CommandLineError;
        }

        request.set_num_cores(cpu_count);
    }

    if (parser->isSet(memOption) || parser->isSet(memOptionDeprecated))
    {
        if (parser->isSet(memOption) && parser->isSet(memOptionDeprecated))
        {
            cerr << "Error: invalid option(s) used for memory allocation. Please use \"--memory\" to specify amount of "
                    "memory to allocate.\n";
            return ParseCode::CommandLineError;
        }

        if (parser->isSet(memOptionDeprecated))
            cerr << "Warning: the \"--mem\" long option is deprecated in favour of \"--memory\". Please update any "
                    "scripts, etc.\n";

        auto arg_mem_size = parser->isSet(memOption) ? parser->value(memOption).toStdString()
                                                     : parser->value(memOptionDeprecated).toStdString();

        mp::MemorySize{arg_mem_size}; // throw if bad

        request.set_mem_size(arg_mem_size);
    }

    if (parser->isSet(diskOption))
    {
        auto arg_disk_size = parser->value(diskOption).toStdString();

        mp::MemorySize{arg_disk_size}; // throw if bad

        request.set_disk_space(arg_disk_size);
    }

    if (parser->isSet(mountOption))
    {
        for (const auto& value : parser->values(mountOption))
        {
            // this is needed so that Windows absolute paths are not split at the colon following the drive letter
            const auto colon_split = QRegularExpression{R"(^[A-Za-z]:[\\/].*)"}.match(value).hasMatch();
            const auto mount_source = value.section(':', 0, colon_split);
            const auto mount_target = value.section(':', colon_split + 1);

            // Validate source directory of client side mounts
            QFileInfo source_dir(mount_source);
            if (!MP_FILEOPS.exists(source_dir))
            {
                cerr << "Mount source path \"" << mount_source.toStdString() << "\" does not exist\n";
                return ParseCode::CommandLineError;
            }

            if (!MP_FILEOPS.isDir(source_dir))
            {
                cerr << "Mount source path \"" << mount_source.toStdString() << "\" is not a directory\n";
                return ParseCode::CommandLineError;
            }

            if (!MP_FILEOPS.isReadable(source_dir))
            {
                cerr << "Mount source path \"" << mount_source.toStdString() << "\" is not readable\n";
                return ParseCode::CommandLineError;
            }

            mount_routes.emplace_back(mount_source, mount_target);
        }
    }

    if (parser->isSet(cloudInitOption))
    {
        constexpr auto err_msg_template = "Could not load cloud-init configuration: {}";
        try
        {
            const QString& cloud_init_file = parser->value(cloudInitOption);
            YAML::Node node;
            if (cloud_init_file == "-")
            {
                node = YAML::Load(term->read_all_cin());
            }
            else if (cloud_init_file.startsWith("http://") || cloud_init_file.startsWith("https://"))
            {
                URLDownloader downloader{std::chrono::minutes{1}};
                auto downloaded_yaml = downloader.download(QUrl(cloud_init_file));
                node = YAML::Load(downloaded_yaml.toStdString());
            }
            else
            {
                auto cloud_init_file_stdstr = cloud_init_file.toStdString();
                auto file_type = fs::status(cloud_init_file_stdstr).type();
                if (file_type != fs::file_type::regular && file_type != fs::file_type::fifo)
                    throw YAML::BadFile{cloud_init_file_stdstr};

                node = YAML::LoadFile(cloud_init_file_stdstr);
            }
            request.set_cloud_init_user_data(YAML::Dump(node));
        }
        catch (const YAML::BadFile& e)
        {
            auto err_detail = fmt::format("{}\n{}", e.what(), "Please ensure that Multipass can read it.");
            fmt::println(cerr, err_msg_template, err_detail);
            return ParseCode::CommandLineError;
        }
        catch (const YAML::Exception& e)
        {
            fmt::println(cerr, err_msg_template, e.what());
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

        instance_name = QString::fromStdString(request.instance_name().empty() ? reply.vm_instance_name()
                                                                               : request.instance_name());

        std::vector<std::string> warning_aliases;
        for (const auto& alias_to_be_created : reply.aliases_to_be_created())
        {
            AliasDefinition alias_definition{alias_to_be_created.instance(), alias_to_be_created.command(),
                                             alias_to_be_created.working_directory()};
            if (create_alias(aliases, alias_to_be_created.name(), alias_definition, cout, cerr,
                             instance_name.toStdString()) != ReturnCode::Ok)
                warning_aliases.push_back(alias_to_be_created.name());
        }

        if (warning_aliases.size())
            cerr << fmt::format("Warning: unable to create {} {}.\n",
                                warning_aliases.size() == 1 ? "alias" : "aliases",
                                fmt::join(warning_aliases, ", "));

        for (const auto& workspace_to_be_created : reply.workspaces_to_be_created())
        {
            auto home_dir = mpu::in_multipass_snap() ? QString::fromLocal8Bit(mpu::snap_real_home_dir())
                                                     : MP_STDPATHS.writableLocation(StandardPaths::HomeLocation);
            auto full_path_str = home_dir + "/multipass/" + QString::fromStdString(workspace_to_be_created);

            QDir full_path(full_path_str);
            if (full_path.exists())
            {
                cerr << fmt::format("Folder \"{}\" already exists.\n", full_path_str);
            }
            else
            {
                if (!MP_FILEOPS.mkpath(full_path, full_path_str))
                {
                    cerr << fmt::format("Error creating folder {}. Not mounting.\n", full_path_str);
                    continue;
                }
            }

            if (mount(parser, full_path_str, QString::fromStdString(workspace_to_be_created)) != ReturnCode::Ok)
            {
                cerr << fmt::format("Error mounting folder {}.\n", full_path_str);
            }
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

    auto streaming_callback = [this](mp::LaunchReply& reply,
                                     grpc::ClientReaderWriterInterface<LaunchRequest, LaunchReply>* client) {
        std::unordered_map<int, std::string> progress_messages{
            {LaunchProgress_ProgressTypes_IMAGE, "Retrieving image: "},
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

auto cmd::Launch::mount(const mp::ArgParser* parser, const QString& mount_source, const QString& mount_target)
    -> ReturnCode
{
    const auto full_mount_target = QString{"%1:%2"}.arg(instance_name, mount_target);
    auto ret = run_cmd({"multipass", "mount", mount_source, full_mount_target}, parser, cout, cerr);
    if (ret == Ok)
        cout << fmt::format("Mounted '{}' into '{}'\n", mount_source, full_mount_target);

    return ret;
}

bool cmd::Launch::ask_bridge_permission(multipass::LaunchReply& reply)
{
    std::vector<std::string> nets;

    std::copy(reply.nets_need_bridging().cbegin(), reply.nets_need_bridging().cend(), std::back_inserter(nets));

    mp::BridgePrompter prompter(term);
    return prompter.bridge_prompt(nets);
}
