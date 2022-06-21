/*
 * Copyright (C) 2017-2022 Canonical, Ltd.
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

#include "daemon.h"
#include "base_cloud_init_config.h"
#include "instance_settings_handler.h"

#include <multipass/constants.h>
#include <multipass/exceptions/blueprint_exceptions.h>
#include <multipass/exceptions/create_image_exception.h>
#include <multipass/exceptions/exitless_sshprocess_exception.h>
#include <multipass/exceptions/invalid_memory_size_exception.h>
#include <multipass/exceptions/not_implemented_on_this_backend_exception.h>
#include <multipass/exceptions/sshfs_missing_error.h>
#include <multipass/exceptions/start_exception.h>
#include <multipass/ip_address.h>
#include <multipass/json_writer.h>
#include <multipass/logging/client_logger.h>
#include <multipass/logging/log.h>
#include <multipass/name_generator.h>
#include <multipass/network_interface.h>
#include <multipass/platform.h>
#include <multipass/query.h>
#include <multipass/settings/settings.h>
#include <multipass/ssh/ssh_session.h>
#include <multipass/top_catch_all.h>
#include <multipass/utils.h>
#include <multipass/version.h>
#include <multipass/virtual_machine.h>
#include <multipass/virtual_machine_description.h>
#include <multipass/virtual_machine_factory.h>
#include <multipass/vm_image.h>
#include <multipass/vm_image_host.h>
#include <multipass/vm_image_vault.h>

#include <multipass/format.h>
#include <yaml-cpp/yaml.h>

#include <QDir>
#include <QEventLoop>
#include <QFutureSynchronizer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QString>
#include <QSysInfo>
#include <QtConcurrent/QtConcurrent>

#include <algorithm>
#include <cassert>
#include <functional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace mp = multipass;
namespace mpl = multipass::logging;
namespace mpu = multipass::utils;

namespace
{

using namespace std::chrono_literals;

using error_string = std::string;

constexpr auto category = "daemon";
constexpr auto instance_db_name = "multipassd-vm-instances.json";
constexpr auto reboot_cmd = "sudo reboot";
constexpr auto stop_ssh_cmd = "sudo systemctl stop ssh";
const std::string sshfs_error_template = "Error enabling mount support in '{}'"
                                         "\n\nPlease install the 'multipass-sshfs' snap manually inside the instance.";

// Images which cannot be bridged with --network.
const std::unordered_set<std::string> no_bridging_release = { // images to check from release and daily remotes
    "10.04",  "lucid", "11.10", "oneiric", "12.04",  "precise", "12.10",  "quantal", "13.04",
    "raring", "13.10", "saucy", "14.04",   "trusty", "14.10",   "utopic", "15.04",   "vivid",
    "15.10",  "wily",  "16.04", "xenial",  "16.10",  "yakkety", "17.04",  "zesty"};
const std::unordered_set<std::string> no_bridging_remote = {};                     // images with other remote specified
const std::unordered_set<std::string> no_bridging_remoteless = {"core", "core16"}; // images which do not use remote

mp::Query query_from(const mp::LaunchRequest* request, const std::string& name)
{
    if (!request->remote_name().empty() && request->image().empty())
        throw std::runtime_error("Must specify an image when specifying a remote");

    std::string image = request->image().empty() ? "default" : request->image();
    // TODO: persistence should be specified by the rpc as well

    mp::Query::Type query_type{mp::Query::Type::Alias};

    if (QString::fromStdString(image).startsWith("file"))
        query_type = mp::Query::Type::LocalFile;
    else if (QString::fromStdString(image).startsWith("http"))
        query_type = mp::Query::Type::HttpDownload;

    return {name, image, false, request->remote_name(), query_type, true};
}

auto make_cloud_init_vendor_config(const mp::SSHKeyProvider& key_provider, const std::string& time_zone,
                                   const std::string& username, const std::string& backend_version_string)
{
    auto ssh_key_line = fmt::format("ssh-rsa {} {}@localhost", key_provider.public_key_as_base64(), username);

    auto config = YAML::Load(mp::base_cloud_init_config);
    config["ssh_authorized_keys"].push_back(ssh_key_line);
    config["timezone"] = time_zone;
    config["system_info"]["default_user"]["name"] = username;

    auto pollinate_user_agent_string =
        fmt::format("multipass/version/{} # written by Multipass\n", multipass::version_string);
    pollinate_user_agent_string += fmt::format("multipass/driver/{} # written by Multipass\n", backend_version_string);
    pollinate_user_agent_string +=
        fmt::format("multipass/host/{} # written by Multipass\n", multipass::platform::host_version());

    YAML::Node pollinate_user_agent_node;
    pollinate_user_agent_node["path"] = "/etc/pollinate/add-user-agent";
    pollinate_user_agent_node["content"] = pollinate_user_agent_string;

    config["write_files"].push_back(pollinate_user_agent_node);

    return config;
}

auto make_cloud_init_meta_config(const std::string& name)
{
    YAML::Node meta_data;

    meta_data["instance-id"] = name;
    meta_data["local-hostname"] = name;
    meta_data["cloud-name"] = "multipass";

    return meta_data;
}

auto make_cloud_init_network_config(const std::string default_mac_addr,
                                    const std::vector<mp::NetworkInterface>& extra_interfaces)
{
    YAML::Node network_data;

    // Generate the cloud-init file only if there is at least one extra interface needing auto configuration.
    if (std::find_if(extra_interfaces.begin(), extra_interfaces.end(),
                     [](const auto& iface) { return iface.auto_mode; }) != extra_interfaces.end())
    {
        network_data["version"] = "2";

        std::string name = "default";
        network_data["ethernets"][name]["match"]["macaddress"] = default_mac_addr;
        network_data["ethernets"][name]["dhcp4"] = true;

        for (size_t i = 0; i < extra_interfaces.size(); ++i)
        {
            if (extra_interfaces[i].auto_mode)
            {
                name = "extra" + std::to_string(i);
                network_data["ethernets"][name]["match"]["macaddress"] = extra_interfaces[i].mac_address;
                network_data["ethernets"][name]["dhcp4"] = true;
                // We make the default gateway associated with the first interface.
                network_data["ethernets"][name]["dhcp4-overrides"]["route-metric"] = 200;
                // Make the interface optional, which means that networkd will not wait for the device to be configured.
                network_data["ethernets"][name]["optional"] = true;
            }
        }
    }

    return network_data;
}

void prepare_user_data(YAML::Node& user_data_config, YAML::Node& vendor_config)
{
    auto users = user_data_config["users"];
    if (users.IsSequence())
        users.push_back("default");

    auto keys = user_data_config["ssh_authorized_keys"];
    if (keys.IsSequence())
        keys.push_back(vendor_config["ssh_authorized_keys"][0]);
}

template <typename T>
auto name_from(const std::string& requested_name, const std::string& blueprint_name, mp::NameGenerator& name_gen,
               const T& currently_used_names)
{
    if (!requested_name.empty())
    {
        return requested_name;
    }
    else if (!blueprint_name.empty())
    {
        return blueprint_name;
    }
    else
    {
        auto name = name_gen.make_name();
        constexpr int num_retries = 100;
        for (int i = 0; i < num_retries; i++)
        {
            if (currently_used_names.find(name) != currently_used_names.end())
                continue;
            return name;
        }
        throw std::runtime_error("unable to generate a unique name");
    }
}

std::vector<mp::NetworkInterface> read_extra_interfaces(const QJsonObject& record)
{
    // Read the extra networks interfaces, if any.
    std::vector<mp::NetworkInterface> extra_interfaces;

    if (record.contains("extra_interfaces"))
    {
        for (QJsonValueRef entry : record["extra_interfaces"].toArray())
        {
            auto id = entry.toObject()["id"].toString().toStdString();
            auto mac_address = entry.toObject()["mac_address"].toString().toStdString();
            if (!mpu::valid_mac_address(mac_address))
            {
                throw std::runtime_error(fmt::format("Invalid MAC address {}", mac_address));
            }
            auto auto_mode = entry.toObject()["auto_mode"].toBool();
            extra_interfaces.push_back(mp::NetworkInterface{id, mac_address, auto_mode});
        }
    }

    return extra_interfaces;
}

std::unordered_map<std::string, mp::VMSpecs> load_db(const mp::Path& data_path, const mp::Path& cache_path)
{
    QDir data_dir{data_path};
    QDir cache_dir{cache_path};
    QFile db_file{data_dir.filePath(instance_db_name)};
    if (!db_file.open(QIODevice::ReadOnly))
    {
        // Try to open the old location
        db_file.setFileName(cache_dir.filePath(instance_db_name));
        if (!db_file.open(QIODevice::ReadOnly))
            return {};
    }

    QJsonParseError parse_error;
    auto doc = QJsonDocument::fromJson(db_file.readAll(), &parse_error);
    if (doc.isNull())
        return {};

    auto records = doc.object();
    if (records.isEmpty())
        return {};

    std::unordered_map<std::string, mp::VMSpecs> reconstructed_records;
    for (auto it = records.constBegin(); it != records.constEnd(); ++it)
    {
        auto key = it.key().toStdString();
        auto record = it.value().toObject();
        if (record.isEmpty())
            return {};

        auto num_cores = record["num_cores"].toInt();
        auto mem_size = record["mem_size"].toString().toStdString();
        auto disk_space = record["disk_space"].toString().toStdString();
        auto ssh_username = record["ssh_username"].toString().toStdString();
        auto state = record["state"].toInt();
        auto deleted = record["deleted"].toBool();
        auto metadata = record["metadata"].toObject();

        if (!num_cores && !deleted && ssh_username.empty() && metadata.isEmpty() &&
            !mp::MemorySize{mem_size}.in_bytes() && !mp::MemorySize{disk_space}.in_bytes())
        {
            mpl::log(mpl::Level::warning, category, fmt::format("Ignoring ghost instance in database: {}", key));
            continue;
        }

        if (ssh_username.empty())
            ssh_username = "ubuntu";

        // Read the default network interface, constructed from the "mac_addr" field.
        auto default_mac_address = record["mac_addr"].toString().toStdString();
        if (!mpu::valid_mac_address(default_mac_address))
        {
            throw std::runtime_error(fmt::format("Invalid MAC address {}", default_mac_address));
        }

        std::unordered_map<std::string, mp::VMMount> mounts;
        mp::id_mappings uid_mappings;
        mp::id_mappings gid_mappings;

        for (QJsonValueRef entry : record["mounts"].toArray())
        {
            auto target_path = entry.toObject()["target_path"].toString().toStdString();
            auto source_path = entry.toObject()["source_path"].toString().toStdString();

            for (QJsonValueRef uid_entry : entry.toObject()["uid_mappings"].toArray())
            {
                uid_mappings.push_back(
                    {uid_entry.toObject()["host_uid"].toInt(), uid_entry.toObject()["instance_uid"].toInt()});
            }

            for (QJsonValueRef gid_entry : entry.toObject()["gid_mappings"].toArray())
            {
                gid_mappings.push_back(
                    {gid_entry.toObject()["host_gid"].toInt(), gid_entry.toObject()["instance_gid"].toInt()});
            }

            mp::VMMount mount{source_path, gid_mappings, uid_mappings};
            mounts[target_path] = mount;
        }

        reconstructed_records[key] = {num_cores,
                                      mp::MemorySize{mem_size.empty() ? mp::default_memory_size : mem_size},
                                      mp::MemorySize{disk_space.empty() ? mp::default_disk_size : disk_space},
                                      default_mac_address,
                                      read_extra_interfaces(record),
                                      ssh_username,
                                      static_cast<mp::VirtualMachine::State>(state),
                                      mounts,
                                      deleted,
                                      metadata};
    }
    return reconstructed_records;
}

auto fetch_image_for(const std::string& name, const mp::FetchType& fetch_type, mp::VMImageVault& vault)
{
    auto stub_prepare = [](const mp::VMImage&) -> mp::VMImage { return {}; };
    auto stub_progress = [](int download_type, int progress) { return true; };

    mp::Query query{name, "", false, "", mp::Query::Type::Alias, false};

    return vault.fetch_image(fetch_type, query, stub_prepare, stub_progress);
}

auto try_mem_size(const std::string& val) -> mp::optional<mp::MemorySize>
{
    try
    {
        return mp::MemorySize{val};
    }
    catch (mp::InvalidMemorySizeException& /*unused*/)
    {
        return mp::nullopt;
    }
}

std::vector<mp::NetworkInterface> validate_extra_interfaces(const mp::LaunchRequest* request,
                                                            const mp::VirtualMachineFactory& factory,
                                                            std::vector<std::string>& nets_need_bridging,
                                                            mp::LaunchError& option_errors)
{
    std::vector<mp::NetworkInterface> interfaces;

    mp::optional<std::vector<mp::NetworkInterfaceInfo>> factory_networks = mp::nullopt;

    bool dont_allow_auto = false;
    std::string specified_image;

    auto remote = request->remote_name();
    auto image = request->image();

    if (request->remote_name().empty())
    {
        specified_image = image;

        dont_allow_auto = (no_bridging_remoteless.find(image) != no_bridging_remoteless.end()) ||
                          (no_bridging_release.find(image) != no_bridging_release.end());
    }
    else
    {
        specified_image = remote + ":" + image;

        dont_allow_auto = no_bridging_remote.find(specified_image) != no_bridging_remote.end();

        if (!dont_allow_auto && (remote == "release" || remote == "daily"))
            dont_allow_auto = no_bridging_release.find(image) != no_bridging_release.end();
    }

    for (const auto& net : request->network_options())
    {
        auto net_id = net.id();

        if (net_id == mp::bridged_network_name)
        {
            const auto bridged_id = MP_SETTINGS.get(mp::bridged_interface_key);
            if (bridged_id == "")
                throw std::runtime_error(
                    fmt::format("You have to `multipass set {}=<name>` to use the `--bridged` shortcut.",
                                mp::bridged_interface_key));
            net_id = bridged_id.toStdString();
        }

        if (!factory_networks)
        {
            try
            {
                factory_networks = factory.networks();
            }
            catch (const mp::NotImplementedOnThisBackendException&)
            {
                throw mp::NotImplementedOnThisBackendException("bridging");
            }
        }

        if (dont_allow_auto && net.mode() == multipass::LaunchRequest_NetworkOptions_Mode_AUTO)
        {
            throw std::runtime_error(fmt::format(
                "Automatic network configuration not available for {}. Consider using manual mode.", specified_image));
        }

        // Check that the id the user specified is valid.
        auto pred = [net_id](const mp::NetworkInterfaceInfo& info) { return info.id == net_id; };
        auto host_net_it = std::find_if(factory_networks->cbegin(), factory_networks->cend(), pred);

        if (host_net_it == factory_networks->cend())
        {
            if (net.id() == mp::bridged_network_name)
                throw std::runtime_error(
                    fmt::format("Invalid network '{}' set as bridged interface, use `multipass set {}=<name>` to "
                                "correct. See `multipass networks` for valid names.",
                                net_id, mp::bridged_interface_key));

            mpl::log(mpl::Level::warning, category, fmt::format("Invalid network name \"{}\"", net_id));
            option_errors.add_error_codes(mp::LaunchError::INVALID_NETWORK);
        }
        else if (host_net_it->needs_authorization)
            nets_need_bridging.push_back(host_net_it->id);

        // In case the user specified a MAC address, check it is valid.
        if (const auto& mac = QString::fromStdString(net.mac_address()).toLower().toStdString();
            mac.empty() || mpu::valid_mac_address(mac))
            interfaces.push_back(
                mp::NetworkInterface{net_id, mac, net.mode() != multipass::LaunchRequest_NetworkOptions_Mode_MANUAL});
        else
        {
            mpl::log(mpl::Level::warning, category, fmt::format("Invalid MAC address \"{}\"", mac));
            option_errors.add_error_codes(mp::LaunchError::INVALID_NETWORK);
        }
    }

    return interfaces;
}

void validate_image(const mp::LaunchRequest* request, const mp::VMImageVault& vault,
                    mp::VMBlueprintProvider& blueprint_provider)
{
    // TODO: Refactor this in such a way that we can use info returned here instead of ignoring it to avoid calls
    //       later that accomplish the same thing.
    try
    {
        blueprint_provider.info_for(request->image());
    }
    catch (const mp::IncompatibleBlueprintException&)
    {
        throw std::runtime_error(
            fmt::format("The \"{}\" Blueprint is not compatible with this host.", request->image()));
    }
    catch (const std::out_of_range&)
    {
        auto image_query = query_from(request, "");
        if (image_query.query_type == mp::Query::Type::Alias)
            vault.all_info_for(image_query);
    }
}

auto validate_create_arguments(const mp::LaunchRequest* request, const mp::DaemonConfig* config)
{
    assert(config && config->factory && config->blueprint_provider && config->vault && "null ptr somewhere...");
    validate_image(request, *config->vault, *config->blueprint_provider);

    static const auto min_mem = try_mem_size(mp::min_memory_size);
    static const auto min_disk = try_mem_size(mp::min_disk_size);
    assert(min_mem && min_disk);

    auto mem_size_str = request->mem_size();
    auto disk_space_str = request->disk_space();
    auto instance_name = request->instance_name();
    auto option_errors = mp::LaunchError{};

    const auto opt_mem_size = try_mem_size(mem_size_str.empty() ? mp::default_memory_size : mem_size_str);

    mp::MemorySize mem_size{};
    if (opt_mem_size && *opt_mem_size >= min_mem)
        mem_size = *opt_mem_size;
    else
        option_errors.add_error_codes(mp::LaunchError::INVALID_MEM_SIZE);

    // If the user did not specify a disk size, then mp::nullopt be passed down. Otherwise, the specified size will be
    // checked.
    mp::optional<mp::MemorySize> disk_space{}; // mp::nullopt by default.
    if (!disk_space_str.empty())
    {
        auto opt_disk_space = try_mem_size(disk_space_str);
        if (opt_disk_space && *opt_disk_space >= min_disk)
        {
            disk_space = opt_disk_space;
        }
        else
        {
            option_errors.add_error_codes(mp::LaunchError::INVALID_DISK_SIZE);
        }
    }

    if (!instance_name.empty() && !mp::utils::valid_hostname(instance_name))
        option_errors.add_error_codes(mp::LaunchError::INVALID_HOSTNAME);

    std::vector<std::string> nets_need_bridging;
    auto extra_interfaces = validate_extra_interfaces(request, *config->factory, nets_need_bridging, option_errors);

    struct CheckedArguments
    {
        mp::MemorySize mem_size;
        mp::optional<mp::MemorySize> disk_space;
        std::string instance_name;
        std::vector<mp::NetworkInterface> extra_interfaces;
        std::vector<std::string> nets_need_bridging;
        mp::LaunchError option_errors;
    } ret{std::move(mem_size),         std::move(disk_space),         std::move(instance_name),
          std::move(extra_interfaces), std::move(nets_need_bridging), std::move(option_errors)};
    return ret;
}

auto grpc_status_for_mount_error(const std::string& instance_name)
{
    return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, fmt::format(sshfs_error_template, instance_name));
}

auto grpc_status_for(fmt::memory_buffer& errors)
{
    if (!errors.size())
        return grpc::Status::OK;

    // Remove trailing newline due to grpc adding one of it's own
    auto error_string = fmt::to_string(errors);
    if (error_string.back() == '\n')
        error_string.pop_back();

    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                        fmt::format("The following errors occurred:\n{}", error_string), "");
}

auto connect_rpc(mp::DaemonRpc& rpc, mp::Daemon& daemon)
{
    QObject::connect(&rpc, &mp::DaemonRpc::on_create, &daemon, &mp::Daemon::create);
    QObject::connect(&rpc, &mp::DaemonRpc::on_launch, &daemon, &mp::Daemon::launch);
    QObject::connect(&rpc, &mp::DaemonRpc::on_purge, &daemon, &mp::Daemon::purge);
    QObject::connect(&rpc, &mp::DaemonRpc::on_find, &daemon, &mp::Daemon::find);
    QObject::connect(&rpc, &mp::DaemonRpc::on_info, &daemon, &mp::Daemon::info);
    QObject::connect(&rpc, &mp::DaemonRpc::on_list, &daemon, &mp::Daemon::list);
    QObject::connect(&rpc, &mp::DaemonRpc::on_networks, &daemon, &mp::Daemon::networks);
    QObject::connect(&rpc, &mp::DaemonRpc::on_mount, &daemon, &mp::Daemon::mount);
    QObject::connect(&rpc, &mp::DaemonRpc::on_recover, &daemon, &mp::Daemon::recover);
    QObject::connect(&rpc, &mp::DaemonRpc::on_ssh_info, &daemon, &mp::Daemon::ssh_info);
    QObject::connect(&rpc, &mp::DaemonRpc::on_start, &daemon, &mp::Daemon::start);
    QObject::connect(&rpc, &mp::DaemonRpc::on_stop, &daemon, &mp::Daemon::stop);
    QObject::connect(&rpc, &mp::DaemonRpc::on_suspend, &daemon, &mp::Daemon::suspend);
    QObject::connect(&rpc, &mp::DaemonRpc::on_restart, &daemon, &mp::Daemon::restart);
    QObject::connect(&rpc, &mp::DaemonRpc::on_delete, &daemon, &mp::Daemon::delet);
    QObject::connect(&rpc, &mp::DaemonRpc::on_umount, &daemon, &mp::Daemon::umount);
    QObject::connect(&rpc, &mp::DaemonRpc::on_version, &daemon, &mp::Daemon::version);
    QObject::connect(&rpc, &mp::DaemonRpc::on_get, &daemon, &mp::Daemon::get);
    QObject::connect(&rpc, &mp::DaemonRpc::on_set, &daemon, &mp::Daemon::set);
    QObject::connect(&rpc, &mp::DaemonRpc::on_keys, &daemon, &mp::Daemon::keys);
    QObject::connect(&rpc, &mp::DaemonRpc::on_authenticate, &daemon, &mp::Daemon::authenticate);
}

template <typename Instances, typename InstanceMap, typename InstanceCheck>
grpc::Status validate_requested_instances(const Instances& instances, const InstanceMap& vms,
                                          InstanceCheck check_instance)
{
    fmt::memory_buffer errors;
    for (const auto& name : instances)
        fmt::format_to(errors, check_instance(name));

    return grpc_status_for(errors);
}

template <typename Instances, typename InstanceMap, typename InstanceCheck>
auto find_requested_instances(const Instances& instances, const InstanceMap& vms, InstanceCheck check_instance)
    -> std::pair<std::vector<typename Instances::value_type>, grpc::Status>
{ // TODO: use this in commands that currently duplicate the same kind of code
    auto status = validate_requested_instances(instances, vms, check_instance);
    auto valid_instances = std::vector<typename Instances::value_type>{};

    if (status.ok())
    {
        if (instances.empty())
            for (const auto& vm_item : vms)
                valid_instances.push_back(vm_item.first);
        else
            std::copy(std::cbegin(instances), std::cend(instances), std::back_inserter(valid_instances));
    }

    return std::make_pair(valid_instances, status);
}

template <typename Instances, typename InstanceMap>
auto find_instances_to_delete(const Instances& instances, const InstanceMap& operational_vms,
                              const InstanceMap& trashed_vms)
    -> std::tuple<std::vector<typename Instances::value_type>, std::vector<typename Instances::value_type>,
                  grpc::Status>
{
    fmt::memory_buffer errors;
    std::vector<typename Instances::value_type> operational_instances_to_delete, trashed_instances_to_delete;

    for (const auto& name : instances)
        if (operational_vms.find(name) != operational_vms.end())
            operational_instances_to_delete.push_back(name);
        else if (trashed_vms.find(name) != trashed_vms.end())
            trashed_instances_to_delete.push_back(name);
        else
            fmt::format_to(errors, "instance \"{}\" does not exist\n", name);

    auto status = grpc_status_for(errors);

    if (status.ok() && operational_instances_to_delete.empty() && trashed_instances_to_delete.empty())
    { // target all instances
        const auto get_first = [](const auto& pair) { return pair.first; };
        std::transform(std::cbegin(operational_vms), std::cend(operational_vms),
                       std::back_inserter(operational_instances_to_delete), get_first);
        std::transform(std::cbegin(trashed_vms), std::cend(trashed_vms),
                       std::back_inserter(trashed_instances_to_delete), get_first);
    }

    return std::make_tuple(operational_instances_to_delete, trashed_instances_to_delete, status);
}

template <typename Instances>
auto instances_running(const Instances& instances)
{
    for (const auto& instance : instances)
    {
        if (mp::utils::is_running(instance.second->current_state()))
            return true;
    }

    return false;
}

grpc::Status stop_accepting_ssh_connections(mp::SSHSession& session)
{
    auto proc = session.exec(stop_ssh_cmd);
    auto ecode = proc.exit_code();

    return ecode == 0 ? grpc::Status::OK
                      : grpc::Status{grpc::StatusCode::FAILED_PRECONDITION,
                                     fmt::format("Could not stop sshd. '{}' exited with code {}", stop_ssh_cmd, ecode),
                                     proc.read_std_error()};
}

grpc::Status ssh_reboot(const std::string& hostname, int port, const std::string& username,
                        const mp::SSHKeyProvider& key_provider)
{
    mp::SSHSession session{hostname, port, username, key_provider};

    // This allows us to later detect when the machine has finished restarting by waiting for SSH to be back up.
    // Otherwise, there would be a race condition, and we would be unable to distinguish whether it had ever been down.
    stop_accepting_ssh_connections(session);

    auto proc = session.exec(reboot_cmd);
    try
    {
        auto ecode = proc.exit_code();

        if (ecode != 0)
            return grpc::Status{grpc::StatusCode::FAILED_PRECONDITION,
                                fmt::format("Reboot command exited with code {}", ecode), proc.read_std_error()};
    }
    catch (const mp::ExitlessSSHProcessException&)
    {
        // this is the expected path
    }

    return grpc::Status::OK;
}

mp::InstanceStatus::Status grpc_instance_status_for(const mp::VirtualMachine::State& state)
{
    switch (state)
    {
    case mp::VirtualMachine::State::off:
    case mp::VirtualMachine::State::stopped:
        return mp::InstanceStatus::STOPPED;
    case mp::VirtualMachine::State::starting:
        return mp::InstanceStatus::STARTING;
    case mp::VirtualMachine::State::restarting:
        return mp::InstanceStatus::RESTARTING;
    case mp::VirtualMachine::State::running:
        return mp::InstanceStatus::RUNNING;
    case mp::VirtualMachine::State::delayed_shutdown:
        return mp::InstanceStatus::DELAYED_SHUTDOWN;
    case mp::VirtualMachine::State::suspending:
        return mp::InstanceStatus::SUSPENDING;
    case mp::VirtualMachine::State::suspended:
        return mp::InstanceStatus::SUSPENDED;
    case mp::VirtualMachine::State::unknown:
    default:
        return mp::InstanceStatus::UNKNOWN;
    }
}

// Computes the final size of an image, but also checks if the value given by the user is bigger than or equal than
// the size of the image.
mp::MemorySize compute_final_image_size(const mp::MemorySize image_size,
                                        mp::optional<mp::MemorySize> command_line_value, mp::Path data_directory)
{
    mp::MemorySize disk_space{};

    if (!command_line_value)
    {
        auto default_disk_size_as_struct = mp::MemorySize(mp::default_disk_size);
        disk_space = image_size < default_disk_size_as_struct ? default_disk_size_as_struct : image_size;
    }
    else if (*command_line_value < image_size)
    {
        throw std::runtime_error(fmt::format("Requested disk ({} bytes) below minimum for this image ({} bytes)",
                                             command_line_value->in_bytes(), image_size.in_bytes()));
    }
    else
    {
        disk_space = *command_line_value;
    }

    auto available_bytes = MP_UTILS.filesystem_bytes_available(data_directory);
    if (available_bytes == -1)
    {
        throw std::runtime_error(fmt::format("Failed to determine information about the volume containing {}",
                                             data_directory.toStdString()));
    }
    std::string available_bytes_str = QString::number(available_bytes).toStdString();
    auto available_disk_space = mp::MemorySize(available_bytes_str + "B");

    if (available_disk_space < image_size)
    {
        throw std::runtime_error(fmt::format("Available disk ({} bytes) below minimum for this image ({} bytes)",
                                             available_disk_space.in_bytes(), image_size.in_bytes()));
    }

    if (available_disk_space < disk_space)
    {
        mpl::log(mpl::Level::warning, category,
                 fmt::format("Reserving more disk space ({} bytes) than available ({} bytes)", disk_space.in_bytes(),
                             available_disk_space.in_bytes()));
    }

    return disk_space;
}

std::unordered_set<std::string> mac_set_from(const mp::VMSpecs& spec)
{
    std::unordered_set<std::string> macs{};

    macs.insert(spec.default_mac_address);

    for (const auto& extra_iface : spec.extra_interfaces)
        macs.insert(extra_iface.mac_address);

    return macs;
}

// Merge the contents of t into s, iff the sets are disjoint (i.e. make s = sUt). Return whether s and t were disjoint.
bool merge_if_disjoint(std::unordered_set<std::string>& s, const std::unordered_set<std::string>& t)
{
    if (any_of(cbegin(s), cend(s), [&t](const auto& mac) { return t.find(mac) != cend(t); }))
        return false;

    s.insert(cbegin(t), cend(t));
    return true;
}

// Generate a MAC address which does not exist in the set s. Then add the address to s.
std::string generate_unused_mac_address(std::unordered_set<std::string>& s)
{
    // TODO: Checking in our list of MAC addresses does not suffice to conclude the generated MAC is unique. We
    // should also check in the ARP table.
    static constexpr auto max_tries = 5;
    for (auto i = 0; i < max_tries; ++i)
        if (auto [it, success] = s.insert(mp::utils::generate_mac_address()); success)
            return *it;

    throw std::runtime_error{
        fmt::format("Failed to generate an unique mac address after {} attempts. Number of mac addresses in use: {}",
                    max_tries, s.size())};
}

bool is_ipv4_valid(const std::string& ipv4)
{
    try
    {
        (mp::IPAddress(ipv4));
    }
    catch (std::invalid_argument&)
    {
        return false;
    }

    return true;
}

void add_aliases(mp::FindReply& response, const std::string& remote_name, const mp::VMImageInfo& info,
                 const std::string& default_remote)
{
    if (!info.aliases.empty())
    {
        auto entry = response.add_images_info();
        for (const auto& alias : info.aliases)
        {
            auto alias_entry = entry->add_aliases_info();
            if (remote_name != default_remote)
            {
                alias_entry->set_remote_name(remote_name);
            }
            alias_entry->set_alias(alias.toStdString());
        }

        entry->set_os(info.os.toStdString());
        entry->set_release(info.release_title.toStdString());
        entry->set_version(info.version.toStdString());
    }
}

auto timeout_for(const int requested_timeout, const int blueprint_timeout)
{
    if (requested_timeout > 0)
        return std::chrono::seconds(requested_timeout);

    if (blueprint_timeout > 0)
        return std::chrono::seconds(blueprint_timeout);

    return mp::default_timeout;
}

mp::SettingsHandler*
register_instance_mod(std::unordered_map<std::string, mp::VMSpecs>& vm_instance_specs,
                      std::unordered_map<std::string, mp::VirtualMachine::ShPtr>& vm_instances,
                      const std::unordered_map<std::string, mp::VirtualMachine::ShPtr>& deleted_instances,
                      const std::unordered_set<std::string>& preparing_instances,
                      std::function<void()> instance_persister)
{
    return MP_SETTINGS.register_handler(std::make_unique<mp::InstanceSettingsHandler>(
        vm_instance_specs, vm_instances, deleted_instances, preparing_instances, std::move(instance_persister)));
}

} // namespace

mp::Daemon::Daemon(std::unique_ptr<const DaemonConfig> the_config)
    : config{std::move(the_config)},
      vm_instance_specs{load_db(
          mp::utils::backend_directory_path(config->data_directory, config->factory->get_backend_directory_name()),
          mp::utils::backend_directory_path(config->cache_directory, config->factory->get_backend_directory_name()))},
      daemon_rpc{config->server_address, *config->cert_provider, config->client_cert_store.get()},
      instance_mounts{*config->ssh_key_provider},
      instance_mod_handler{register_instance_mod(vm_instance_specs, vm_instances, deleted_instances,
                                                 preparing_instances, [this] { persist_instances(); })}
{
    connect_rpc(daemon_rpc, *this);
    std::vector<std::string> invalid_specs;

    try
    {
        config->factory->hypervisor_health_check();
    }
    catch (const std::runtime_error& e)
    {
        mpl::log(mpl::Level::warning, category, fmt::format("Hypervisor health check failed: {}", e.what()));
    }

    for (auto& entry : vm_instance_specs)
    {
        const auto& name = entry.first;
        auto& spec = entry.second;

        if (!config->vault->has_record_for(name))
        {
            invalid_specs.push_back(name);
            continue;
        }

        // Check that all the interfaces in the instance have different MAC address, and that they were not used in
        // the other instances. String validity was already checked in load_db(). Add these MAC's to the daemon's set
        // only if this instance is not invalid.
        auto new_macs = mac_set_from(spec);

        if (new_macs.size() <= spec.extra_interfaces.size() || !merge_if_disjoint(new_macs, allocated_mac_addrs))
        {
            // There is at least one repeated address in new_macs.
            mpl::log(mpl::Level::warning, category, fmt::format("{} has repeated MAC addresses", name));
            invalid_specs.push_back(name);
            continue;
        }

        auto vm_image = fetch_image_for(name, config->factory->fetch_type(), *config->vault);
        if (!vm_image.image_path.isEmpty() && !QFile::exists(vm_image.image_path))
        {
            mpl::log(mpl::Level::warning, category,
                     fmt::format("Could not find image for '{}'. Expected location: {}", name, vm_image.image_path));
            invalid_specs.push_back(name);
            continue;
        }

        const auto instance_dir = mp::utils::base_dir(vm_image.image_path);
        const auto cloud_init_iso = instance_dir.filePath("cloud-init-config.iso");
        mp::VirtualMachineDescription vm_desc{spec.num_cores,
                                              spec.mem_size,
                                              spec.disk_space,
                                              name,
                                              spec.default_mac_address,
                                              spec.extra_interfaces,
                                              spec.ssh_username,
                                              vm_image,
                                              cloud_init_iso,
                                              {},
                                              {},
                                              {},
                                              {}};

        auto& instance_record = spec.deleted ? deleted_instances : vm_instances;
        instance_record[name] = config->factory->create_virtual_machine(vm_desc, *this);

        allocated_mac_addrs = std::move(new_macs); // Add the new macs to the daemon's list only if we got this far

        // FIXME: somehow we're writing contradictory state to disk.
        if (spec.deleted && spec.state != VirtualMachine::State::stopped)
        {
            mpl::log(mpl::Level::warning, category,
                     fmt::format("{} is deleted but has incompatible state {}, resetting state to 0 (stopped)", name,
                                 static_cast<int>(spec.state)));
            spec.state = VirtualMachine::State::stopped;
        }

        if (spec.state == VirtualMachine::State::running && vm_instances[name]->state != VirtualMachine::State::running)
        {
            assert(!spec.deleted);
            mpl::log(mpl::Level::info, category, fmt::format("{} needs starting. Starting now...", name));

            QTimer::singleShot(0, [this, &name] {
                multipass::top_catch_all(name, [this, &name]() {
                    vm_instances[name]->start();
                    on_restart(name);
                });
            });
        }
    }

    for (const auto& bad_spec : invalid_specs)
    {
        mpl::log(mpl::Level::warning, category, fmt::format("Removing invalid instance: {}", bad_spec));
        vm_instance_specs.erase(bad_spec);
    }

    if (!invalid_specs.empty())
        persist_instances();

    config->vault->prune_expired_images();

    // Fire timer every six hours to perform maintenance on source images such as
    // pruning expired images and updating to newly released images.
    connect(&source_images_maintenance_task, &QTimer::timeout, [this]() {
        if (image_update_future.isRunning())
        {
            mpl::log(mpl::Level::info, category, "Image updater already running. Skipping…");
        }
        else
        {
            image_update_future = QtConcurrent::run([this] {
                config->vault->prune_expired_images();

                auto prepare_action = [this](const VMImage& source_image) -> VMImage {
                    return config->factory->prepare_source_image(source_image);
                };

                auto download_monitor = [](int download_type, int percentage) {
                    static int last_percentage_logged = -1;
                    if (percentage % 10 == 0)
                    {
                        // Note: The progress callback may be called repeatedly with the same percentage,
                        // so this logic is to only log it once
                        if (last_percentage_logged != percentage)
                        {
                            mpl::log(mpl::Level::info, category, fmt::format("  {}%", percentage));
                            last_percentage_logged = percentage;
                        }
                    }
                    return true;
                };
                try
                {
                    config->vault->update_images(config->factory->fetch_type(), prepare_action, download_monitor);
                }
                catch (const std::exception& e)
                {
                    mpl::log(mpl::Level::error, category, fmt::format("Error updating images: {}", e.what()));
                }
            });
        }
    });
    source_images_maintenance_task.start(config->image_refresh_timer);
}

mp::Daemon::~Daemon()
{
    mp::top_catch_all(category, [this] { MP_SETTINGS.unregister_handler(instance_mod_handler); });
}

void mp::Daemon::create(const CreateRequest* request, grpc::ServerWriterInterface<CreateReply>* server,
                        std::promise<grpc::Status>* status_promise) // clang-format off
try // clang-format on
{
    mpl::ClientLogger<CreateReply> logger{mpl::level_from(request->verbosity_level()), *config->logger, server};
    return create_vm(request, server, status_promise, /*start=*/false);
}
catch (const std::exception& e)
{
    status_promise->set_value(grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, e.what(), ""));
}

void mp::Daemon::launch(const LaunchRequest* request, grpc::ServerWriterInterface<LaunchReply>* server,
                        std::promise<grpc::Status>* status_promise) // clang-format off
try // clang-format on
{
    mpl::ClientLogger<LaunchReply> logger{mpl::level_from(request->verbosity_level()), *config->logger, server};

    return create_vm(request, server, status_promise, /*start=*/true);
}
catch (const mp::StartException& e)
{
    auto name = e.name();

    release_resources(name);
    vm_instances.erase(name);
    persist_instances();

    status_promise->set_value(grpc::Status(grpc::StatusCode::ABORTED, e.what(), ""));
}
catch (const std::exception& e)
{
    status_promise->set_value(grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, e.what(), ""));
}

void mp::Daemon::purge(const PurgeRequest* request, grpc::ServerWriterInterface<PurgeReply>* server,
                       std::promise<grpc::Status>* status_promise) // clang-format off
try // clang-format on
{
    PurgeReply response;

    for (const auto& del : deleted_instances)
    {
        release_resources(del.first);
        response.add_purged_instances(del.first);
    }

    deleted_instances.clear();
    persist_instances();

    server->Write(response);
    status_promise->set_value(grpc::Status::OK);
}
catch (const std::exception& e)
{
    status_promise->set_value(grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, e.what(), ""));
}

void mp::Daemon::find(const FindRequest* request, grpc::ServerWriterInterface<FindReply>* server,
                      std::promise<grpc::Status>* status_promise) // clang-format off
try // clang-format on
{
    mpl::ClientLogger<FindReply> logger{mpl::level_from(request->verbosity_level()), *config->logger, server};
    FindReply response;
    const auto default_remote{"release"};

    if (!request->search_string().empty())
    {
        std::vector<std::pair<std::string, VMImageInfo>> vm_images_info;

        try
        {
            vm_images_info = config->vault->all_info_for({"", request->search_string(), false, request->remote_name(),
                                                          Query::Type::Alias, request->allow_unsupported()});
        }
        catch (const std::exception&)
        {
            try
            {
                vm_images_info.push_back(
                    std::make_pair("", config->blueprint_provider->info_for(request->search_string())));
            }
            catch (const std::exception&)
            {
                throw std::runtime_error(
                    fmt::format("Unable to find an image or Blueprint matching \"{}\"", request->search_string()));
            }
        }

        for (const auto& [remote, info] : vm_images_info)
        {
            std::string name;
            if (info.aliases.contains(QString::fromStdString(request->search_string())))
            {
                name = request->search_string();
            }
            else
            {
                name = info.id.toStdString();
                name.resize(12);
            }

            auto entry = response.add_images_info();
            entry->set_os(info.os.toStdString());
            entry->set_release(info.release_title.toStdString());
            entry->set_version(info.version.toStdString());
            auto alias_entry = entry->add_aliases_info();
            if (!request->remote_name().empty() ||
                (request->remote_name().empty() && vm_images_info.size() > 1 && remote != default_remote))
            {
                alias_entry->set_remote_name(remote);
            }
            alias_entry->set_alias(name);
        }
    }
    else if (request->remote_name().empty())
    {
        for (const auto& image_host : config->image_hosts)
        {
            std::unordered_set<std::string> images_found;
            auto action = [&images_found, &default_remote, request, &response](const std::string& remote,
                                                                               const mp::VMImageInfo& info) {
                if ((info.supported || request->allow_unsupported()) && !info.aliases.empty() &&
                    images_found.find(info.release_title.toStdString()) == images_found.end())
                {
                    add_aliases(response, remote, info, default_remote);
                    images_found.insert(info.release_title.toStdString());
                }
            };

            image_host->for_each_entry_do(action);
        }

        auto vm_blueprints_info = config->blueprint_provider->all_blueprints();

        for (const auto& info : vm_blueprints_info)
        {
            add_aliases(response, "", info, "");
        }
    }
    else
    {
        const auto& remote = request->remote_name();
        auto image_host = config->vault->image_host_for(remote);
        auto vm_images_info = image_host->all_images_for(remote, request->allow_unsupported());

        for (const auto& info : vm_images_info)
        {
            add_aliases(response, remote, info, "");
        }
    }
    server->Write(response);
    status_promise->set_value(grpc::Status::OK);
}
catch (const std::exception& e)
{
    status_promise->set_value(grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, e.what(), ""));
}

void mp::Daemon::info(const InfoRequest* request, grpc::ServerWriterInterface<InfoReply>* server,
                      std::promise<grpc::Status>* status_promise) // clang-format off
try // clang-format on
{
    mpl::ClientLogger<InfoReply> logger{mpl::level_from(request->verbosity_level()), *config->logger, server};
    InfoReply response;

    fmt::memory_buffer errors;
    bool have_mounts = false;
    std::vector<decltype(vm_instances)::key_type> instances_for_info;

    if (request->instance_names().instance_name().empty())
    {
        for (auto& pair : vm_instances)
            instances_for_info.push_back(pair.first);
    }
    else
    {
        for (const auto& name : request->instance_names().instance_name())
            instances_for_info.push_back(name);
    }

    for (const auto& name : instances_for_info)
    {
        auto it = vm_instances.find(name);
        bool deleted{false};
        if (it == vm_instances.end())
        {
            it = deleted_instances.find(name);
            if (it == deleted_instances.end())
            {
                fmt::format_to(errors, "instance \"{}\" does not exist\n", name);
                continue;
            }
            deleted = true;
        }

        auto info = response.add_info();
        auto& vm = it->second;
        auto present_state = vm->current_state();
        info->set_name(name);
        if (deleted)
        {
            info->mutable_instance_status()->set_status(mp::InstanceStatus::DELETED);
        }
        else
        {
            info->mutable_instance_status()->set_status(grpc_instance_status_for(present_state));
        }

        auto vm_image = fetch_image_for(name, config->factory->fetch_type(), *config->vault);
        auto original_release = vm_image.original_release;

        if (!vm_image.id.empty() && original_release.empty())
        {
            try
            {
                auto vm_image_info = config->image_hosts.back()->info_for_full_hash(vm_image.id);
                original_release = vm_image_info.release_title.toStdString();
            }
            catch (const std::exception& e)
            {
                mpl::log(mpl::Level::warning, category, fmt::format("Cannot fetch image information: {}", e.what()));
            }
        }

        info->set_image_release(original_release);
        info->set_id(vm_image.id);

        auto vm_specs = vm_instance_specs[name];

        auto mount_info = info->mutable_mount_info();

        mount_info->set_longest_path_len(0);

        if (!vm_specs.mounts.empty())
            have_mounts = true;

        if (MP_SETTINGS.get_as<bool>(mp::mounts_key))
        {
            for (const auto& mount : vm_specs.mounts)
            {
                if (mount.second.source_path.size() > mount_info->longest_path_len())
                {
                    mount_info->set_longest_path_len(mount.second.source_path.size());
                }

                auto entry = mount_info->add_mount_paths();
                entry->set_source_path(mount.second.source_path);
                entry->set_target_path(mount.first);

                for (const auto& uid_mapping : mount.second.uid_mappings)
                {
                    auto uid_pair = entry->mutable_mount_maps()->add_uid_mappings();
                    uid_pair->set_host_id(uid_mapping.first);
                    uid_pair->set_instance_id(uid_mapping.second);
                }
                for (const auto& gid_mapping : mount.second.gid_mappings)
                {
                    auto gid_pair = entry->mutable_mount_maps()->add_gid_mappings();
                    gid_pair->set_host_id(gid_mapping.first);
                    gid_pair->set_instance_id(gid_mapping.second);
                }
            }
        }

        if (!request->no_runtime_information() && mp::utils::is_running(present_state))
        {
            mp::SSHSession session{vm->ssh_hostname(), vm->ssh_port(), vm_specs.ssh_username,
                                   *config->ssh_key_provider};

            info->set_load(mpu::run_in_ssh_session(session, "cat /proc/loadavg | cut -d ' ' -f1-3"));
            info->set_memory_usage(mpu::run_in_ssh_session(session, "free -b | grep 'Mem:' | awk '{printf $3}'"));
            info->set_memory_total(mpu::run_in_ssh_session(session, "free -b | grep 'Mem:' | awk '{printf $2}'"));
            info->set_disk_usage(mpu::run_in_ssh_session(
                session, "df --output=used `awk '$2 == \"/\" { print $1 }' /proc/mounts` -B1 | sed 1d"));
            info->set_disk_total(mpu::run_in_ssh_session(
                session, "df --output=size `awk '$2 == \"/\" { print $1 }' /proc/mounts` -B1 | sed 1d"));

            std::string management_ip = vm->management_ipv4();
            auto all_ipv4 = vm->get_all_ipv4(*config->ssh_key_provider);

            if (is_ipv4_valid(management_ip))
                info->add_ipv4(management_ip);
            else if (all_ipv4.empty())
                info->add_ipv4("N/A");

            for (const auto& extra_ipv4 : all_ipv4)
                if (extra_ipv4 != management_ip)
                    info->add_ipv4(extra_ipv4);

            auto current_release = mpu::run_in_ssh_session(session, "lsb_release -ds");
            info->set_current_release(!current_release.empty() ? current_release : original_release);
        }
    }

    if (have_mounts && !MP_SETTINGS.get_as<bool>(mp::mounts_key))
        mpl::log(mpl::Level::error, category, "Mounts have been disabled on this instance of Multipass");

    auto status = grpc_status_for(errors);
    if (status.ok())
        server->Write(response);

    status_promise->set_value(status);
}
catch (const std::exception& e)
{
    status_promise->set_value(grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, e.what(), ""));
}

void mp::Daemon::list(const ListRequest* request, grpc::ServerWriterInterface<ListReply>* server,
                      std::promise<grpc::Status>* status_promise) // clang-format off
try // clang-format on
{
    mpl::ClientLogger<ListReply> logger{mpl::level_from(request->verbosity_level()), *config->logger, server};
    ListReply response;
    config->update_prompt->populate_if_time_to_show(response.mutable_update_info());

    for (const auto& instance : vm_instances)
    {
        const auto& name = instance.first;
        const auto& vm = instance.second;
        auto present_state = vm->current_state();
        auto entry = response.add_instances();
        entry->set_name(name);
        entry->mutable_instance_status()->set_status(grpc_instance_status_for(present_state));

        // FIXME: Set the release to the cached current version when supported
        auto vm_image = fetch_image_for(name, config->factory->fetch_type(), *config->vault);
        auto current_release = vm_image.original_release;

        if (!vm_image.id.empty() && current_release.empty())
        {
            try
            {
                auto vm_image_info = config->image_hosts.back()->info_for_full_hash(vm_image.id);
                current_release = vm_image_info.release_title.toStdString();
            }
            catch (const std::exception& e)
            {
                mpl::log(mpl::Level::warning, category, fmt::format("Cannot fetch image information: {}", e.what()));
            }
        }

        entry->set_current_release(current_release);

        if (request->request_ipv4() && mp::utils::is_running(present_state))
        {
            std::string management_ip = vm->management_ipv4();
            auto all_ipv4 = vm->get_all_ipv4(*config->ssh_key_provider);

            if (is_ipv4_valid(management_ip))
                entry->add_ipv4(management_ip);
            else if (all_ipv4.empty())
                entry->add_ipv4("N/A");

            for (const auto& extra_ipv4 : all_ipv4)
                if (extra_ipv4 != management_ip)
                    entry->add_ipv4(extra_ipv4);
        }
    }

    for (const auto& instance : deleted_instances)
    {
        const auto& name = instance.first;
        auto entry = response.add_instances();
        entry->set_name(name);
        entry->mutable_instance_status()->set_status(mp::InstanceStatus::DELETED);
    }

    server->Write(response);
    status_promise->set_value(grpc::Status::OK);
}
catch (const std::exception& e)
{
    status_promise->set_value(grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, e.what(), ""));
}

void mp::Daemon::networks(const NetworksRequest* request, grpc::ServerWriterInterface<NetworksReply>* server,
                          std::promise<grpc::Status>* status_promise) // clang-format off
try // clang-format on
{
    mpl::ClientLogger<NetworksReply> logger{mpl::level_from(request->verbosity_level()), *config->logger, server};
    NetworksReply response;
    config->update_prompt->populate_if_time_to_show(response.mutable_update_info());

    if (!instances_running(vm_instances))
        config->factory->hypervisor_health_check();

    const auto& iface_list = config->factory->networks();

    for (const auto& iface : iface_list)
    {
        auto entry = response.add_interfaces();
        entry->set_name(iface.id);
        entry->set_type(iface.type);
        entry->set_description(iface.description);
    }

    server->Write(response);
    status_promise->set_value(grpc::Status::OK);
}
catch (const std::exception& e)
{
    status_promise->set_value(grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, e.what(), ""));
}

void mp::Daemon::mount(const MountRequest* request, grpc::ServerWriterInterface<MountReply>* server,
                       std::promise<grpc::Status>* status_promise) // clang-format off
try // clang-format on
{
    mpl::ClientLogger<MountReply> logger{mpl::level_from(request->verbosity_level()), *config->logger, server};

    if (!MP_SETTINGS.get_as<bool>(mp::mounts_key))
    {
        return status_promise->set_value(
            grpc::Status(grpc::StatusCode::FAILED_PRECONDITION,
                         "Mounts are disabled on this installation of Multipass.\n\n"
                         "See https://multipass.run/docs/set-command#local.privileged-mounts for information\n"
                         "on how to enable them."));
    }

    QFileInfo source_dir(QString::fromStdString(request->source_path()));
    if (!source_dir.exists())
    {
        return status_promise->set_value(
            grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                         fmt::format("source \"{}\" does not exist", request->source_path()), ""));
    }

    if (!source_dir.isDir())
    {
        return status_promise->set_value(
            grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                         fmt::format("source \"{}\" is not a directory", request->source_path()), ""));
    }

    if (!source_dir.isReadable())
    {
        return status_promise->set_value(
            grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                         fmt::format("source \"{}\" is not readable", request->source_path()), ""));
    }

    mp::id_mappings uid_mappings, gid_mappings;

    auto mount_maps = request->mount_maps();

    for (auto i = 0; i < mount_maps.uid_mappings_size(); ++i)
    {
        auto map_pair = mount_maps.uid_mappings(i);
        uid_mappings.push_back({map_pair.host_id(), map_pair.instance_id()});
    }

    for (auto i = 0; i < mount_maps.gid_mappings_size(); ++i)
    {
        auto map_pair = mount_maps.gid_mappings(i);
        gid_mappings.push_back({map_pair.host_id(), map_pair.instance_id()});
    }

    fmt::memory_buffer errors;
    for (const auto& path_entry : request->target_paths())
    {
        const auto name = path_entry.instance_name();
        auto it = vm_instances.find(name);
        if (it == vm_instances.end())
        {
            fmt::format_to(errors, "instance \"{}\" does not exist\n", name);
            continue;
        }

        auto target_path = path_entry.target_path();
        if (mp::utils::invalid_target_path(QString::fromStdString(target_path)))
        {
            fmt::format_to(errors, "Unable to mount to \"{}\"\n", target_path);
            continue;
        }

        if (instance_mounts.has_instance_already_mounted(name, target_path))
        {
            fmt::format_to(errors, "\"{}:{}\" is already mounted\n", name, target_path);
            continue;
        }

        auto& vm = it->second;
        auto& vm_specs = vm_instance_specs[name];

        if (vm->current_state() == mp::VirtualMachine::State::running)
        {
            try
            {
                instance_mounts.start_mount(vm.get(), request->source_path(), target_path, gid_mappings, uid_mappings);
            }
            catch (const mp::SSHFSMissingError&)
            {
                try
                {
                    // Force the deleteLater() event to process now to avoid unloading the apparmor profile
                    // later.  See https://github.com/canonical/multipass/issues/1131
                    QCoreApplication::sendPostedEvents(0, QEvent::DeferredDelete);

                    MountReply mount_reply;
                    mount_reply.set_mount_message("Enabling support for mounting");
                    server->Write(mount_reply);

                    mp::SSHSession session{vm->ssh_hostname(), vm->ssh_port(), vm_specs.ssh_username,
                                           *config->ssh_key_provider};
                    mp::utils::install_sshfs_for(name, session);
                    instance_mounts.start_mount(vm.get(), request->source_path(), target_path, gid_mappings,
                                                uid_mappings);
                }
                catch (const mp::SSHFSMissingError&)
                {
                    return status_promise->set_value(grpc_status_for_mount_error(name));
                }
            }
            catch (const std::exception& e)
            {
                fmt::format_to(errors, "error mounting \"{}\": {}", target_path, e.what());
                continue;
            }
        }

        if (vm_specs.mounts.find(target_path) != vm_specs.mounts.end())
        {
            fmt::format_to(errors, "There is already a mount defined for \"{}:{}\"\n", name, target_path);
            continue;
        }

        VMMount mount{request->source_path(), gid_mappings, uid_mappings};
        vm_specs.mounts[target_path] = mount;
    }

    persist_instances();

    status_promise->set_value(grpc_status_for(errors));
}
catch (const std::exception& e)
{
    status_promise->set_value(grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, e.what(), ""));
}

void mp::Daemon::recover(const RecoverRequest* request, grpc::ServerWriterInterface<RecoverReply>* server,
                         std::promise<grpc::Status>* status_promise) // clang-format off
try // clang-format on
{
    mpl::ClientLogger<RecoverReply> logger{mpl::level_from(request->verbosity_level()), *config->logger, server};

    const auto [instances, status] =
        find_requested_instances(request->instance_names().instance_name(), deleted_instances,
                                 std::bind(&Daemon::check_instance_exists, this, std::placeholders::_1));

    if (status.ok())
    {
        for (const auto& name : instances)
        {
            auto it = deleted_instances.find(name);
            if (it != std::end(deleted_instances))
            {
                assert(vm_instance_specs[name].deleted);
                vm_instance_specs[name].deleted = false;
                vm_instances[name] = std::move(it->second);
                deleted_instances.erase(it);
            }
            else
            {
                mpl::log(mpl::Level::debug, category,
                         fmt::format("instance \"{}\" does not need to be recovered", name));
            }
        }

        persist_instances();
    }

    status_promise->set_value(status);
}
catch (const std::exception& e)
{
    status_promise->set_value(grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, e.what(), ""));
}

void mp::Daemon::ssh_info(const SSHInfoRequest* request, grpc::ServerWriterInterface<SSHInfoReply>* server,
                          std::promise<grpc::Status>* status_promise) // clang-format off
try // clang-format on
{
    mpl::ClientLogger<SSHInfoReply> logger{mpl::level_from(request->verbosity_level()), *config->logger, server};
    SSHInfoReply response;

    for (const auto& name : request->instance_name())
    {
        auto it = vm_instances.find(name);
        if (it == vm_instances.end())
        {
            if (deleted_instances.find(name) == deleted_instances.end())
                return status_promise->set_value(
                    grpc::Status{grpc::StatusCode::NOT_FOUND, fmt::format("instance \"{}\" does not exist", name)});
            else
                return status_promise->set_value(
                    grpc::Status{grpc::StatusCode::INVALID_ARGUMENT, fmt::format("instance \"{}\" is deleted", name)});
        }

        auto& vm = it->second;
        if (vm->current_state() == VirtualMachine::State::unknown)
            throw std::runtime_error("Cannot retrieve credentials in unknown state");

        if (!mp::utils::is_running(vm->current_state()))
        {
            return status_promise->set_value(
                grpc::Status(grpc::StatusCode::ABORTED, fmt::format("instance \"{}\" is not running", name)));
        }

        if (vm->state == VirtualMachine::State::delayed_shutdown)
        {
            if (delayed_shutdown_instances[name]->get_time_remaining() <= std::chrono::minutes(1))
            {
                return status_promise->set_value(
                    grpc::Status(grpc::StatusCode::FAILED_PRECONDITION,
                                 fmt::format("\"{}\" is scheduled to shut down in less than a minute, use "
                                             "'multipass stop --cancel {}' to cancel the shutdown.",
                                             name, name),
                                 ""));
            }
        }

        mp::SSHInfo ssh_info;
        ssh_info.set_host(vm->ssh_hostname());
        ssh_info.set_port(vm->ssh_port());
        ssh_info.set_priv_key_base64(config->ssh_key_provider->private_key_as_base64());
        ssh_info.set_username(vm->ssh_username());
        (*response.mutable_ssh_info())[name] = ssh_info;
    }

    server->Write(response);
    status_promise->set_value(grpc::Status::OK);
}
catch (const std::exception& e)
{
    status_promise->set_value(grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, e.what(), ""));
}

void mp::Daemon::start(const StartRequest* request, grpc::ServerWriterInterface<StartReply>* server,
                       std::promise<grpc::Status>* status_promise) // clang-format off
try // clang-format on
{
    mpl::ClientLogger<StartReply> logger{mpl::level_from(request->verbosity_level()), *config->logger, server};

    auto timeout = request->timeout() > 0 ? std::chrono::seconds(request->timeout()) : mp::default_timeout;

    if (!instances_running(vm_instances))
        config->factory->hypervisor_health_check();

    mp::StartError start_error;
    auto* errors = start_error.mutable_instance_errors();
    bool have_mounts = false;

    std::vector<decltype(vm_instances)::key_type> vms;
    for (const auto& name : request->instance_names().instance_name())
    {
        auto it = vm_instances.find(name);
        if (it == vm_instances.end())
            errors->insert({name, deleted_instances.find(name) == deleted_instances.end()
                                      ? mp::StartError::DOES_NOT_EXIST
                                      : mp::StartError::INSTANCE_DELETED});
        else
        {
            if (!vm_instance_specs[name].mounts.empty())
                have_mounts = true;
            if (it->second->current_state() == VirtualMachine::State::delayed_shutdown)
                delayed_shutdown_instances.erase(name);
            else if (it->second->current_state() != VirtualMachine::State::running)
                vms.push_back(name);
        }
    }

    if (start_error.instance_errors_size())
        return status_promise->set_value(
            grpc::Status(grpc::StatusCode::ABORTED, "instance(s) missing", start_error.SerializeAsString()));

    if (have_mounts && !MP_SETTINGS.get_as<bool>(mp::mounts_key))
        mpl::log(mpl::Level::error, category, "Mounts have been disabled on this instance of Multipass");

    if (request->instance_names().instance_name().empty())
    {
        for (auto& pair : vm_instances)
        {
            if (pair.second->current_state() == VirtualMachine::State::running)
                continue;
            vms.push_back(pair.first);
        }
    }

    for (const auto& name : vms)
    {
        auto it = vm_instances.find(name);
        auto state = it->second->current_state();
        if (state != VirtualMachine::State::starting && state != VirtualMachine::State::restarting)
            it->second->start();
    }

    auto future_watcher = create_future_watcher();
    future_watcher->setFuture(
        QtConcurrent::run(this, &Daemon::async_wait_for_ready_all<StartReply>, server, vms, timeout, status_promise));
}
catch (const std::exception& e)
{
    status_promise->set_value(grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, e.what(), ""));
}

void mp::Daemon::stop(const StopRequest* request, grpc::ServerWriterInterface<StopReply>* server,
                      std::promise<grpc::Status>* status_promise) // clang-format off
try // clang-format on
{
    mpl::ClientLogger<StopReply> logger{mpl::level_from(request->verbosity_level()), *config->logger, server};

    auto [instances, status] =
        find_requested_instances(request->instance_names().instance_name(), vm_instances,
                                 std::bind(&Daemon::check_instance_operational, this, std::placeholders::_1));

    if (status.ok())
    {
        std::function<grpc::Status(VirtualMachine&)> operation;
        if (request->cancel_shutdown())
            operation = std::bind(&Daemon::cancel_vm_shutdown, this, std::placeholders::_1);
        else
            operation = std::bind(&Daemon::shutdown_vm, this, std::placeholders::_1,
                                  std::chrono::minutes(request->time_minutes()));

        status = cmd_vms(instances, operation);
    }

    status_promise->set_value(status);
}
catch (const std::exception& e)
{
    status_promise->set_value(grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, e.what(), ""));
}

void mp::Daemon::suspend(const SuspendRequest* request, grpc::ServerWriterInterface<SuspendReply>* server,
                         std::promise<grpc::Status>* status_promise) // clang-format off
try // clang-format on
{
    mpl::ClientLogger<SuspendReply> logger{mpl::level_from(request->verbosity_level()), *config->logger, server};

    fmt::memory_buffer errors;
    std::vector<decltype(vm_instances)::key_type> instances_to_suspend;
    for (const auto& name : request->instance_names().instance_name())
    {
        auto it = vm_instances.find(name);
        if (it == vm_instances.end())
        {
            it = deleted_instances.find(name);
            if (it == deleted_instances.end())
                fmt::format_to(errors, "instance \"{}\" does not exist\n", name);
            else
                fmt::format_to(errors, "instance \"{}\" is deleted\n", name);
            continue;
        }
        instances_to_suspend.push_back(name);
    }

    auto status = grpc_status_for(errors);
    if (status.ok())
    {
        if (instances_to_suspend.empty())
        {
            for (auto& pair : vm_instances)
                instances_to_suspend.push_back(pair.first);
        }

        status = cmd_vms(instances_to_suspend, [this](auto& vm) {
            vm.suspend();
            instance_mounts.stop_all_mounts_for_instance(vm.vm_name);
            return grpc::Status::OK;
        });
    }

    status_promise->set_value(status);
}
catch (const std::exception& e)
{
    status_promise->set_value(grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, e.what(), ""));
}

void mp::Daemon::restart(const RestartRequest* request, grpc::ServerWriterInterface<RestartReply>* server,
                         std::promise<grpc::Status>* status_promise) // clang-format off
try // clang-format on
{
    mpl::ClientLogger<RestartReply> logger{mpl::level_from(request->verbosity_level()), *config->logger, server};

    auto timeout = request->timeout() > 0 ? std::chrono::seconds(request->timeout()) : mp::default_timeout;

    auto [instances, status] =
        find_requested_instances(request->instance_names().instance_name(), vm_instances,
                                 std::bind(&Daemon::check_instance_operational, this, std::placeholders::_1));

    if (!status.ok())
    {
        return status_promise->set_value(status);
    }

    status = cmd_vms(instances,
                     std::bind(&Daemon::reboot_vm, this, std::placeholders::_1)); // 1st pass to reboot all targets

    if (!status.ok())
    {
        return status_promise->set_value(status);
    }

    auto future_watcher = create_future_watcher();
    future_watcher->setFuture(QtConcurrent::run(this, &Daemon::async_wait_for_ready_all<RestartReply>, server,
                                                instances, timeout, status_promise));
}
catch (const std::exception& e)
{
    status_promise->set_value(grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, e.what(), ""));
}

void mp::Daemon::delet(const DeleteRequest* request, grpc::ServerWriterInterface<DeleteReply>* server,
                       std::promise<grpc::Status>* status_promise) // clang-format off
try // clang-format on
{
    mpl::ClientLogger<DeleteReply> logger{mpl::level_from(request->verbosity_level()), *config->logger, server};
    DeleteReply response;

    const auto [operational_instances_to_delete, trashed_instances_to_delete, status] =
        find_instances_to_delete(request->instance_names().instance_name(), vm_instances, deleted_instances);

    if (status.ok())
    {
        const bool purge = request->purge();

        for (const auto& name : operational_instances_to_delete)
        {
            assert(!vm_instance_specs[name].deleted);

            auto& instance = vm_instances[name];

            if (instance->current_state() == VirtualMachine::State::delayed_shutdown)
                delayed_shutdown_instances.erase(name);

            instance_mounts.stop_all_mounts_for_instance(name);
            instance->shutdown();

            if (purge)
            {
                release_resources(name);
                response.add_purged_instances(name);
            }
            else
            {
                deleted_instances[name] = std::move(instance);
                vm_instance_specs[name].deleted = true;
            }

            vm_instances.erase(name);
        }

        if (purge)
        {
            for (const auto& name : trashed_instances_to_delete)
            {
                assert(vm_instance_specs[name].deleted);
                release_resources(name);
                deleted_instances.erase(name);
                response.add_purged_instances(name);
            }
        }

        persist_instances();
    }

    server->Write(response);
    status_promise->set_value(status);
}
catch (const std::exception& e)
{
    status_promise->set_value(grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, e.what(), ""));
}

void mp::Daemon::umount(const UmountRequest* request, grpc::ServerWriterInterface<UmountReply>* server,
                        std::promise<grpc::Status>* status_promise) // clang-format off
try // clang-format on
{
    mpl::ClientLogger<UmountReply> logger{mpl::level_from(request->verbosity_level()), *config->logger, server};

    fmt::memory_buffer errors;
    for (const auto& path_entry : request->target_paths())
    {
        const auto name = path_entry.instance_name();
        auto it = vm_instances.find(name);
        if (it == vm_instances.end())
        {
            fmt::format_to(errors, "instance \"{}\" does not exist\n", name);
            continue;
        }

        auto target_path = path_entry.target_path();
        auto& mounts = vm_instance_specs[name].mounts;
        auto& vm = it->second;

        // Empty target path indicates removing all mounts for the VM instance
        if (target_path.empty())
        {
            instance_mounts.stop_all_mounts_for_instance(name);
            mounts.clear();
        }
        else
        {
            if (vm->current_state() == mp::VirtualMachine::State::running)
            {
                if (!instance_mounts.stop_mount(name, target_path))
                {
                    fmt::format_to(errors, "\"{}\" is not mounted\n", target_path);
                }
            }

            auto erased = mounts.erase(target_path);
            if (!erased)
            {
                fmt::format_to(errors, "\"{}\" not found in database\n", target_path);
            }
        }
    }

    persist_instances();

    status_promise->set_value(grpc_status_for(errors));
}
catch (const std::exception& e)
{
    status_promise->set_value(grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, e.what(), ""));
}

void mp::Daemon::version(const VersionRequest* request, grpc::ServerWriterInterface<VersionReply>* server,
                         std::promise<grpc::Status>* status_promise)
{
    mpl::ClientLogger<VersionReply> logger{mpl::level_from(request->verbosity_level()), *config->logger, server};

    VersionReply reply;
    reply.set_version(multipass::version_string);
    config->update_prompt->populate(reply.mutable_update_info());
    server->Write(reply);
    status_promise->set_value(grpc::Status::OK);
}

void mp::Daemon::get(const GetRequest* request, grpc::ServerWriterInterface<GetReply>* server,
                     std::promise<grpc::Status>* status_promise)
try
{
    mpl::ClientLogger<GetReply> logger{mpl::level_from(request->verbosity_level()), *config->logger, server};

    GetReply reply;

    auto key = request->key();
    auto val = MP_SETTINGS.get(QString::fromStdString(key)).toStdString();
    mpl::log(mpl::Level::debug, category, fmt::format("Returning setting {}={}", key, val));

    reply.set_value(val);
    server->Write(reply);
    status_promise->set_value(grpc::Status::OK);
}
catch (const mp::UnrecognizedSettingException& e)
{
    status_promise->set_value(grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, e.what(), ""));
}
catch (const std::exception& e)
{
    status_promise->set_value(grpc::Status(grpc::StatusCode::INTERNAL, e.what(), ""));
}

void mp::Daemon::set(const SetRequest* request, grpc::ServerWriterInterface<SetReply>* server,
                     std::promise<grpc::Status>* status_promise)
try
{
    mpl::ClientLogger<SetReply> logger{mpl::level_from(request->verbosity_level()), *config->logger, server};

    auto key = request->key();
    auto val = request->val();

    mpl::log(mpl::Level::trace, category, fmt::format("Trying to set {}={}", key, val));
    MP_SETTINGS.set(QString::fromStdString(key), QString::fromStdString(val));
    mpl::log(mpl::Level::debug, category, fmt::format("Succeeded setting {}={}", key, val));

    status_promise->set_value(grpc::Status::OK);
}
catch (const mp::UnrecognizedSettingException& e)
{
    status_promise->set_value(grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, e.what(), ""));
}
catch (const mp::InvalidSettingException& e)
{
    status_promise->set_value(grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, e.what(), ""));
}
catch (const std::exception& e)
{
    status_promise->set_value(grpc::Status(grpc::StatusCode::INTERNAL, e.what(), ""));
}

void mp::Daemon::keys(const mp::KeysRequest* request, grpc::ServerWriterInterface<KeysReply>* server,
                      std::promise<grpc::Status>* status_promise)
try
{
    mpl::ClientLogger<KeysReply> logger{mpl::level_from(request->verbosity_level()), *config->logger, server};

    KeysReply reply;

    for (const auto& key : MP_SETTINGS.keys())
        reply.add_settings_keys(key.toStdString());

    mpl::log(mpl::Level::debug, category, fmt::format("Returning {} settings keys", reply.settings_keys_size()));
    server->Write(reply);

    status_promise->set_value(grpc::Status::OK);
}
catch (const std::exception& e)
{
    status_promise->set_value(grpc::Status(grpc::StatusCode::INTERNAL, e.what(), ""));
}

void mp::Daemon::authenticate(const AuthenticateRequest* request,
                              grpc::ServerWriterInterface<AuthenticateReply>* server,
                              std::promise<grpc::Status>* status_promise)
try
{
    mpl::ClientLogger<AuthenticateReply> logger{mpl::level_from(request->verbosity_level()), *config->logger, server};

    auto stored_hash = MP_SETTINGS.get(mp::passphrase_key);

    if (stored_hash.isNull() || stored_hash.isEmpty())
    {
        return status_promise->set_value(
            grpc::Status(grpc::StatusCode::FAILED_PRECONDITION,
                         "Passphrase is not set. Please `multipass set local.passphrase` with a trusted client."));
    }

    auto hashed_passphrase = MP_UTILS.generate_scrypt_hash_for(QString::fromStdString(request->passphrase()));

    if (stored_hash != hashed_passphrase)
    {
        return status_promise->set_value(
            grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Passphrase is not correct. Please try again."));
    }

    status_promise->set_value(grpc::Status::OK);
}
catch (const std::exception& e)
{
    status_promise->set_value(grpc::Status(grpc::StatusCode::INTERNAL, e.what(), ""));
}

void mp::Daemon::on_shutdown()
{
}

void mp::Daemon::on_resume()
{
}

void mp::Daemon::on_stop()
{
}

void mp::Daemon::on_suspend()
{
}

void mp::Daemon::on_restart(const std::string& name)
{
    auto future_watcher = create_future_watcher();
    future_watcher->setFuture(QtConcurrent::run(this, &Daemon::async_wait_for_ready_all<StartReply>, nullptr,
                                                std::vector<std::string>{name}, mp::default_timeout, nullptr));
}

void mp::Daemon::persist_state_for(const std::string& name, const VirtualMachine::State& state)
{
    vm_instance_specs[name].state = state;
    persist_instances();
}

void mp::Daemon::update_metadata_for(const std::string& name, const QJsonObject& metadata)
{
    vm_instance_specs[name].metadata = metadata;

    persist_instances();
}

QJsonObject mp::Daemon::retrieve_metadata_for(const std::string& name)
{
    return vm_instance_specs[name].metadata;
}

QJsonArray to_json_array(const std::vector<mp::NetworkInterface>& extra_interfaces)
{
    QJsonArray json;

    for (const auto& interface : extra_interfaces)
    {
        QJsonObject entry;
        entry.insert("id", QString::fromStdString(interface.id));
        entry.insert("mac_address", QString::fromStdString(interface.mac_address));
        entry.insert("auto_mode", interface.auto_mode);
        json.append(entry);
    }

    return json;
}

void mp::Daemon::persist_instances()
{
    auto vm_spec_to_json = [](const mp::VMSpecs& specs) -> QJsonObject {
        QJsonObject json;
        json.insert("num_cores", specs.num_cores);
        json.insert("mem_size", QString::number(specs.mem_size.in_bytes()));
        json.insert("disk_space", QString::number(specs.disk_space.in_bytes()));
        json.insert("ssh_username", QString::fromStdString(specs.ssh_username));
        json.insert("state", static_cast<int>(specs.state));
        json.insert("deleted", specs.deleted);
        json.insert("metadata", specs.metadata);

        // Write the networking information. Write first a field "mac_addr" containing the MAC address of the
        // default network interface. Then, write all the information about the rest of the interfaces.
        json.insert("mac_addr", QString::fromStdString(specs.default_mac_address));
        json.insert("extra_interfaces", to_json_array(specs.extra_interfaces));

        QJsonArray mounts;
        for (const auto& mount : specs.mounts)
        {
            QJsonObject entry;
            entry.insert("source_path", QString::fromStdString(mount.second.source_path));
            entry.insert("target_path", QString::fromStdString(mount.first));

            QJsonArray uid_mappings;
            for (const auto& map : mount.second.uid_mappings)
            {
                QJsonObject map_entry;
                map_entry.insert("host_uid", map.first);
                map_entry.insert("instance_uid", map.second);

                uid_mappings.append(map_entry);
            }

            entry.insert("uid_mappings", uid_mappings);

            QJsonArray gid_mappings;
            for (const auto& map : mount.second.gid_mappings)
            {
                QJsonObject map_entry;
                map_entry.insert("host_gid", map.first);
                map_entry.insert("instance_gid", map.second);

                gid_mappings.append(map_entry);
            }

            entry.insert("gid_mappings", gid_mappings);
            mounts.append(entry);
        }

        json.insert("mounts", mounts);
        return json;
    };
    QJsonObject instance_records_json;
    for (const auto& record : vm_instance_specs)
    {
        auto key = QString::fromStdString(record.first);
        instance_records_json.insert(key, vm_spec_to_json(record.second));
    }
    QDir data_dir{
        mp::utils::backend_directory_path(config->data_directory, config->factory->get_backend_directory_name())};
    mp::write_json(instance_records_json, data_dir.filePath(instance_db_name));
}

void mp::Daemon::release_resources(const std::string& instance)
{
    config->factory->remove_resources_for(instance);
    config->vault->remove(instance);

    auto spec_it = vm_instance_specs.find(instance);
    if (spec_it != cend(vm_instance_specs))
    {
        for (const auto& mac : mac_set_from(spec_it->second))
            allocated_mac_addrs.erase(mac);

        vm_instance_specs.erase(spec_it);
    }
}

std::string mp::Daemon::check_instance_operational(const std::string& instance_name) const
{
    if (vm_instances.find(instance_name) == std::cend(vm_instances))
    {
        if (deleted_instances.find(instance_name) == std::cend(deleted_instances))
            return fmt::format("instance \"{}\" does not exist\n", instance_name);
        else
            return fmt::format("instance \"{}\" is deleted\n", instance_name);
    }

    return {};
}

std::string mp::Daemon::check_instance_exists(const std::string& instance_name) const
{
    if (vm_instances.find(instance_name) == std::cend(vm_instances) &&
        deleted_instances.find(instance_name) == std::cend(deleted_instances))
        return fmt::format("instance \"{}\" does not exist\n", instance_name);

    return {};
}

void mp::Daemon::create_vm(const CreateRequest* request, grpc::ServerWriterInterface<CreateReply>* server,
                           std::promise<grpc::Status>* status_promise, bool start)
{
    auto checked_args = validate_create_arguments(request, config.get());

    if (!checked_args.option_errors.error_codes().empty())
    {
        return status_promise->set_value(grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Invalid arguments supplied",
                                                      checked_args.option_errors.SerializeAsString()));
    }
    else if (auto& nets = checked_args.nets_need_bridging; !nets.empty() && !request->permission_to_bridge())
    {
        CreateError create_error;
        create_error.add_error_codes(CreateError::INVALID_NETWORK);

        CreateReply reply;
        *reply.mutable_nets_need_bridging() = {std::make_move_iterator(nets.begin()),
                                               std::make_move_iterator(nets.end())}; /* this constructs a temporary
                                               RepeatedPtrField from the range, then move-assigns that temporary in */
        server->Write(reply);

        return status_promise->set_value(
            grpc::Status{grpc::StatusCode::FAILED_PRECONDITION, "Missing bridges", create_error.SerializeAsString()});
    }

    // TODO: We should only need to query the Blueprint Provider once for all info, so this (and timeout below) will
    //       need a refactoring to do so.
    auto name = name_from(checked_args.instance_name, config->blueprint_provider->name_from_blueprint(request->image()),
                          *config->name_generator, vm_instances);

    if (vm_instances.find(name) != vm_instances.end() || deleted_instances.find(name) != deleted_instances.end())
    {
        CreateError create_error;
        create_error.add_error_codes(CreateError::INSTANCE_EXISTS);

        return status_promise->set_value(grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                                                      fmt::format("instance \"{}\" already exists", name),
                                                      create_error.SerializeAsString()));
    }

    if (preparing_instances.find(name) != preparing_instances.end())
    {
        CreateError create_error;
        create_error.add_error_codes(CreateError::INSTANCE_EXISTS);

        return status_promise->set_value(grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                                                      fmt::format("instance \"{}\" is being prepared", name),
                                                      create_error.SerializeAsString()));
    }

    if (!instances_running(vm_instances))
        config->factory->hypervisor_health_check();

    // TODO: We should only need to query the Blueprint Provider once for all info, so this (and name above) will
    //       need a refactoring to do so.
    auto timeout = timeout_for(request->timeout(), config->blueprint_provider->blueprint_timeout(name));

    preparing_instances.insert(name);

    auto prepare_future_watcher = new QFutureWatcher<VirtualMachineDescription>();
    auto log_level = mpl::level_from(request->verbosity_level());

    QObject::connect(
        prepare_future_watcher, &QFutureWatcher<VirtualMachineDescription>::finished,
        [this, server, status_promise, name, timeout, start, prepare_future_watcher, log_level] {
            mpl::ClientLogger<CreateReply> logger{log_level, *config->logger, server};

            try
            {
                auto vm_desc = prepare_future_watcher->future().result();

                vm_instance_specs[name] = {vm_desc.num_cores,
                                           vm_desc.mem_size,
                                           vm_desc.disk_space,
                                           vm_desc.default_mac_address,
                                           vm_desc.extra_interfaces,
                                           config->ssh_username,
                                           VirtualMachine::State::off,
                                           {},
                                           false,
                                           QJsonObject()};
                vm_instances[name] = config->factory->create_virtual_machine(vm_desc, *this);
                preparing_instances.erase(name);

                persist_instances();

                if (start)
                {
                    LaunchReply reply;
                    reply.set_create_message("Starting " + name);
                    server->Write(reply);

                    auto& vm = vm_instances[name];
                    vm->start();

                    auto future_watcher = create_future_watcher([this, server, name] {
                        LaunchReply reply;
                        reply.set_vm_instance_name(name);
                        config->update_prompt->populate_if_time_to_show(reply.mutable_update_info());
                        server->Write(reply);
                    });
                    future_watcher->setFuture(QtConcurrent::run(this, &Daemon::async_wait_for_ready_all<LaunchReply>,
                                                                server, std::vector<std::string>{name}, timeout,
                                                                status_promise));
                }
                else
                {
                    status_promise->set_value(grpc::Status::OK);
                }
            }
            catch (const std::exception& e)
            {
                preparing_instances.erase(name);
                release_resources(name);
                vm_instances.erase(name);
                persist_instances();
                status_promise->set_value(grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, e.what(), ""));
            }

            delete prepare_future_watcher;
        });

    auto make_vm_description = [this, server, request, name, checked_args,
                                log_level]() mutable -> VirtualMachineDescription {
        mpl::ClientLogger<CreateReply> logger{log_level, *config->logger, server};

        try
        {
            CreateReply reply;
            reply.set_create_message("Creating " + name);
            server->Write(reply);

            Query query;
            VirtualMachineDescription vm_desc{
                request->num_cores(),
                MemorySize{request->mem_size().empty() ? "0b" : request->mem_size()},
                MemorySize{request->disk_space().empty() ? "0b" : request->disk_space()},
                name,
                "",
                {},
                config->ssh_username,
                VMImage{},
                "",
                YAML::Node{},
                YAML::Node{},
                make_cloud_init_vendor_config(*config->ssh_key_provider, request->time_zone(), config->ssh_username,
                                              config->factory->get_backend_version_string().toStdString()),
                YAML::Node{}};

            try
            {
                query = config->blueprint_provider->fetch_blueprint_for(request->image(), vm_desc);
                query.name = name;
            }
            catch (const std::out_of_range&)
            {
                // Blueprint not found, move on
                query = query_from(request, name);
                vm_desc.mem_size = checked_args.mem_size;
            }

            auto progress_monitor = [server](int progress_type, int percentage) {
                CreateReply create_reply;
                create_reply.mutable_launch_progress()->set_percent_complete(std::to_string(percentage));
                create_reply.mutable_launch_progress()->set_type((CreateProgress::ProgressTypes)progress_type);
                return server->Write(create_reply);
            };

            auto prepare_action = [this, server, &name](const VMImage& source_image) -> VMImage {
                CreateReply reply;
                reply.set_create_message("Preparing image for " + name);
                server->Write(reply);

                return config->factory->prepare_source_image(source_image);
            };

            auto fetch_type = config->factory->fetch_type();

            auto vm_image = config->vault->fetch_image(fetch_type, query, prepare_action, progress_monitor);

            const auto image_size = config->vault->minimum_image_size_for(vm_image.id);
            vm_desc.disk_space = compute_final_image_size(
                image_size, vm_desc.disk_space.in_bytes() > 0 ? vm_desc.disk_space : checked_args.disk_space,
                config->data_directory);

            reply.set_create_message("Configuring " + name);
            server->Write(reply);

            config->factory->prepare_networking(checked_args.extra_interfaces);

            // This set stores the MAC's which need to be in the allocated_mac_addrs if everything goes well.
            auto new_macs = allocated_mac_addrs;

            // check for repetition of requested macs
            for (auto& iface : checked_args.extra_interfaces)
                if (!iface.mac_address.empty() && !new_macs.insert(iface.mac_address).second)
                    throw std::runtime_error(fmt::format("Repeated MAC address {}", iface.mac_address));

            // generate missing macs in a second pass, to avoid repeating macs that the user requested
            for (auto& iface : checked_args.extra_interfaces)
                if (iface.mac_address.empty())
                    iface.mac_address = generate_unused_mac_address(new_macs);

            vm_desc.default_mac_address = generate_unused_mac_address(new_macs);
            vm_desc.extra_interfaces = checked_args.extra_interfaces;

            vm_desc.meta_data_config = make_cloud_init_meta_config(name);
            vm_desc.user_data_config = YAML::Load(request->cloud_init_user_data());
            prepare_user_data(vm_desc.user_data_config, vm_desc.vendor_data_config);

            if (vm_desc.num_cores < std::stoi(mp::min_cpu_cores))
                vm_desc.num_cores = std::stoi(mp::default_cpu_cores);

            vm_desc.network_data_config =
                make_cloud_init_network_config(vm_desc.default_mac_address, checked_args.extra_interfaces);

            vm_desc.image = vm_image;
            config->factory->configure(vm_desc);
            config->factory->prepare_instance_image(vm_image, vm_desc);

            // Everything went well, add the MAC addresses used in this instance.
            allocated_mac_addrs = std::move(new_macs);
            return vm_desc;
        }
        catch (const std::exception& e)
        {
            throw CreateImageException(e.what());
        }
    };

    prepare_future_watcher->setFuture(QtConcurrent::run(make_vm_description));
}

grpc::Status mp::Daemon::reboot_vm(VirtualMachine& vm)
{
    if (vm.state == VirtualMachine::State::delayed_shutdown)
        delayed_shutdown_instances.erase(vm.vm_name);

    if (!mp::utils::is_running(vm.current_state()))
        return grpc::Status{grpc::StatusCode::INVALID_ARGUMENT,
                            fmt::format("instance \"{}\" is not running", vm.vm_name), ""};

    mpl::log(mpl::Level::debug, category, fmt::format("Rebooting {}", vm.vm_name));
    return ssh_reboot(vm.ssh_hostname(), vm.ssh_port(), vm.ssh_username(), *config->ssh_key_provider);
}

grpc::Status mp::Daemon::shutdown_vm(VirtualMachine& vm, const std::chrono::milliseconds delay)
{
    const auto& name = vm.vm_name;
    const auto& state = vm.current_state();

    using St = VirtualMachine::State;
    const auto skip_states = {St::off, St::stopped, St::suspended};

    if (std::none_of(cbegin(skip_states), cend(skip_states), [&state](const auto& st) { return state == st; }))
    {
        delayed_shutdown_instances.erase(name);

        mp::optional<mp::SSHSession> session;
        try
        {
            session = mp::SSHSession{vm.ssh_hostname(), vm.ssh_port(), vm.ssh_username(), *config->ssh_key_provider};
        }
        catch (const std::exception& e)
        {
            mpl::log(mpl::Level::info, category,
                     fmt::format("Cannot open ssh session on \"{}\" shutdown: {}", name, e.what()));
        }

        auto& shutdown_timer = delayed_shutdown_instances[name] = std::make_unique<DelayedShutdownTimer>(
            &vm, std::move(session),
            std::bind(&SSHFSMounts::stop_all_mounts_for_instance, &instance_mounts, std::placeholders::_1));

        QObject::connect(shutdown_timer.get(), &DelayedShutdownTimer::finished,
                         [this, name]() { delayed_shutdown_instances.erase(name); });

        shutdown_timer->start(delay);
    }
    else
        mpl::log(mpl::Level::debug, category, fmt::format("instance \"{}\" does not need stopping", name));

    return grpc::Status::OK;
}

grpc::Status mp::Daemon::cancel_vm_shutdown(const VirtualMachine& vm)
{
    auto it = delayed_shutdown_instances.find(vm.vm_name);
    if (it != delayed_shutdown_instances.end())
        delayed_shutdown_instances.erase(it);
    else
        mpl::log(mpl::Level::debug, category,
                 fmt::format("no delayed shutdown to cancel on instance \"{}\"", vm.vm_name));

    return grpc::Status::OK;
}

grpc::Status mp::Daemon::cmd_vms(const std::vector<std::string>& tgts, std::function<grpc::Status(VirtualMachine&)> cmd)
{ /* TODO: use this in commands, rather than repeating the same logic.
  std::function involves some overhead, but it should be negligible here and
  it gives clear error messages on type mismatch (!= templated callable). */
    for (const auto& tgt : tgts)
    {
        const auto st = cmd(*vm_instances.at(tgt));
        if (!st.ok())
            return st; // Fail early
    }

    return grpc::Status::OK;
}

QFutureWatcher<mp::Daemon::AsyncOperationStatus>*
mp::Daemon::create_future_watcher(std::function<void()> const& finished_op)
{
    async_future_watchers.emplace_back(std::make_unique<QFutureWatcher<AsyncOperationStatus>>());

    auto future_watcher = async_future_watchers.back().get();
    QObject::connect(future_watcher, &QFutureWatcher<AsyncOperationStatus>::finished,
                     [this, future_watcher, finished_op] {
                         finished_op();
                         finish_async_operation(future_watcher->future());
                     });

    return future_watcher;
}

template <typename Reply>
error_string mp::Daemon::async_wait_for_ssh_and_start_mounts_for(const std::string& name,
                                                                 const std::chrono::seconds& timeout,
                                                                 grpc::ServerWriterInterface<Reply>* server)
{
    fmt::memory_buffer errors;
    try
    {
        auto it = vm_instances.find(name);
        auto vm = it->second;
        vm->wait_until_ssh_up(timeout);

        if (std::is_same<Reply, LaunchReply>::value)
        {
            if (server)
            {
                Reply reply;
                reply.set_reply_message("Waiting for initialization to complete");
                server->Write(reply);
            }

            MP_UTILS.wait_for_cloud_init(vm.get(), timeout, *config->ssh_key_provider);
        }

        if (MP_SETTINGS.get_as<bool>(mp::mounts_key))
        {
            std::vector<std::string> invalid_mounts;
            auto& mounts = vm_instance_specs[name].mounts;
            auto& vm_specs = vm_instance_specs[name];
            for (const auto& mount_entry : mounts)
            {
                auto& target_path = mount_entry.first;
                auto& source_path = mount_entry.second.source_path;
                auto& uid_mappings = mount_entry.second.uid_mappings;
                auto& gid_mappings = mount_entry.second.gid_mappings;

                try
                {
                    instance_mounts.start_mount(vm.get(), source_path, target_path, gid_mappings, uid_mappings);
                }
                catch (const mp::SSHFSMissingError&)
                {
                    try
                    {
                        if (server)
                        {
                            Reply reply;
                            reply.set_reply_message("Enabling support for mounting");
                            server->Write(reply);
                        }

                        mp::SSHSession session{vm->ssh_hostname(), vm->ssh_port(), vm_specs.ssh_username,
                                               *config->ssh_key_provider};
                        mp::utils::install_sshfs_for(name, session);
                        instance_mounts.start_mount(vm.get(), source_path, target_path, gid_mappings, uid_mappings);
                    }
                    catch (const mp::SSHFSMissingError&)
                    {
                        fmt::format_to(errors, sshfs_error_template + "\n", name);
                        break;
                    }
                }
                catch (const std::exception& e)
                {
                    fmt::format_to(errors, "Removing \"{}\": {}\n", target_path, e.what());
                    invalid_mounts.push_back(target_path);
                }
                persist_instances();
            }
        }
    }
    catch (const std::exception& e)
    {
        fmt::format_to(errors, e.what());
    }

    return fmt::to_string(errors);
}

template <typename Reply>
mp::Daemon::AsyncOperationStatus
mp::Daemon::async_wait_for_ready_all(grpc::ServerWriterInterface<Reply>* server, const std::vector<std::string>& vms,
                                     const std::chrono::seconds& timeout, std::promise<grpc::Status>* status_promise)
{
    QFutureSynchronizer<std::string> start_synchronizer;
    {
        std::lock_guard<decltype(start_mutex)> lock{start_mutex};
        for (const auto& name : vms)
        {
            if (async_running_futures.find(name) != async_running_futures.end())
            {
                start_synchronizer.addFuture(async_running_futures[name]);
            }
            else
            {
                auto future = QtConcurrent::run(this, &Daemon::async_wait_for_ssh_and_start_mounts_for<Reply>, name,
                                                timeout, server);
                async_running_futures[name] = future;
                start_synchronizer.addFuture(future);
            }
        }
    }

    start_synchronizer.waitForFinished();

    {
        std::lock_guard<decltype(start_mutex)> lock{start_mutex};
        for (const auto& name : vms)
        {
            async_running_futures.erase(name);
        }
    }

    fmt::memory_buffer errors;
    for (const auto& future : start_synchronizer.futures())
    {
        auto error = future.result();
        if (!error.empty())
        {
            fmt::format_to(errors, "{}\n", error);
        }
    }

    if (server && std::is_same<Reply, StartReply>::value)
    {
        if (config->update_prompt->is_time_to_show())
        {
            Reply reply;
            config->update_prompt->populate(reply.mutable_update_info());
            server->Write(reply);
        }
    }

    return {grpc_status_for(errors), status_promise};
}

void mp::Daemon::finish_async_operation(QFuture<AsyncOperationStatus> async_future)
{
    auto it = std::find_if(async_future_watchers.begin(), async_future_watchers.end(),
                           [&async_future](const std::unique_ptr<QFutureWatcher<AsyncOperationStatus>>& watcher) {
                               return watcher->future() == async_future;
                           });

    if (it != async_future_watchers.end())
    {
        async_future_watchers.erase(it);
    }

    auto async_op_result = async_future.result();

    if (!async_op_result.status.ok())
        persist_instances();

    if (async_op_result.status_promise)
        async_op_result.status_promise->set_value(async_op_result.status);
}
