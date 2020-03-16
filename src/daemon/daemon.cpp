/*
 * Copyright (C) 2017-2020 Canonical, Ltd.
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
#include "json_writer.h"

#include <multipass/cloud_init_iso.h>
#include <multipass/constants.h>
#include <multipass/exceptions/create_image_exception.h>
#include <multipass/exceptions/exitless_sshprocess_exception.h>
#include <multipass/exceptions/invalid_memory_size_exception.h>
#include <multipass/exceptions/sshfs_missing_error.h>
#include <multipass/exceptions/start_exception.h>
#include <multipass/logging/client_logger.h>
#include <multipass/logging/log.h>
#include <multipass/name_generator.h>
#include <multipass/platform.h>
#include <multipass/query.h>
#include <multipass/ssh/ssh_session.h>
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
#include <QSysInfo>
#include <QtConcurrent/QtConcurrent>

#include <cassert>
#include <functional>
#include <stdexcept>
#include <utility>

namespace mp = multipass;
namespace mpl = multipass::logging;

namespace
{

using namespace std::chrono_literals;

using error_string = std::string;

constexpr auto category = "daemon";
constexpr auto instance_db_name = "multipassd-vm-instances.json";
constexpr auto uuid_file_name = "multipass-unique-id";
constexpr auto metrics_opt_in_file = "multipassd-send-metrics.yaml";
constexpr auto reboot_cmd = "sudo reboot";
constexpr auto up_timeout = 2min; // This may be tweaked as appropriate and used in places that wait for ssh to be up
constexpr auto cloud_init_timeout = 5min;
constexpr auto stop_ssh_cmd = "sudo systemctl stop ssh";
constexpr auto max_install_sshfs_retries = 3;
const std::string sshfs_error_template = "Error enabling mount support in '{}'"
                                         "\n\nPlease install 'sshfs' manually inside the instance.";

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
    pollinate_user_agent_string += fmt::format("multipass/host/{}-{} # written by Multipass\n", QSysInfo::productType(),
                                               QSysInfo::productVersion());

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

    return meta_data;
}

auto emit_yaml(YAML::Node& node, const std::string& node_name)
{
    YAML::Emitter emitter;
    emitter.SetIndent(4);
    emitter << node;
    if (!emitter.good())
        throw std::runtime_error{
            fmt::format("Failed to emit {} cloud-init config: {}", node_name, emitter.GetLastError())};

    return fmt::format("#cloud-config\n{}\n", emitter.c_str());
}

auto make_cloud_init_image(const std::string& name, const QDir& instance_dir, YAML::Node& meta_data_config,
                           YAML::Node& user_data_config, YAML::Node& vendor_data_config)
{
    const auto cloud_init_iso = instance_dir.filePath("cloud-init-config.iso");
    if (QFile::exists(cloud_init_iso))
        return cloud_init_iso;

    mp::CloudInitIso iso;
    iso.add_file("meta-data", emit_yaml(meta_data_config, "meta data"));
    iso.add_file("vendor-data", emit_yaml(vendor_data_config, "vendor data"));
    iso.add_file("user-data", emit_yaml(user_data_config, "user data"));
    iso.write_to(cloud_init_iso);

    return cloud_init_iso;
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

mp::VirtualMachineDescription to_machine_desc(const mp::LaunchRequest* request, const std::string& name,
                                              const mp::MemorySize& mem_size, const mp::MemorySize& disk_space,
                                              const std::string& mac_addr, const std::string& ssh_username,
                                              const mp::VMImage& image, YAML::Node& meta_data_config,
                                              YAML::Node& user_data_config, YAML::Node& vendor_data_config)
{
    const auto num_cores = request->num_cores() < std::stoi(mp::min_cpu_cores)
                               ? std::stoi(mp::default_cpu_cores)
                               : request->num_cores();
    const auto instance_dir = mp::utils::base_dir(image.image_path);
    const auto cloud_init_iso =
        make_cloud_init_image(name, instance_dir, meta_data_config, user_data_config, vendor_data_config);
    return {num_cores, mem_size, disk_space, name, mac_addr, ssh_username, image, cloud_init_iso};
}

template <typename T>
auto name_from(const std::string& requested_name, mp::NameGenerator& name_gen, const T& currently_used_names)
{
    if (requested_name.empty())
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
    return requested_name;
}

std::unordered_map<std::string, mp::VMSpecs> load_db(const mp::Path& data_path, const mp::Path& cache_path)
{
    QDir data_dir{data_path};
    QDir cache_dir{cache_path};
    QFile db_file{data_dir.filePath(instance_db_name)};
    auto opened = db_file.open(QIODevice::ReadOnly);
    if (!opened)
    {
        // Try to open the old location
        QDir cache_dir{cache_path};
        db_file.setFileName(cache_dir.filePath(instance_db_name));
        auto opened = db_file.open(QIODevice::ReadOnly);
        if (!opened)
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
        auto mac_addr = record["mac_addr"].toString().toStdString();
        auto ssh_username = record["ssh_username"].toString().toStdString();
        auto state = record["state"].toInt();
        auto deleted = record["deleted"].toBool();
        auto metadata = record["metadata"].toObject();

        if (ssh_username.empty())
            ssh_username = "ubuntu";

        std::unordered_map<std::string, mp::VMMount> mounts;
        std::unordered_map<int, int> uid_map;
        std::unordered_map<int, int> gid_map;

        for (const auto& entry : record["mounts"].toArray())
        {
            auto target_path = entry.toObject()["target_path"].toString().toStdString();
            auto source_path = entry.toObject()["source_path"].toString().toStdString();

            for (const auto& uid_entry : entry.toObject()["uid_mappings"].toArray())
            {
                uid_map[uid_entry.toObject()["host_uid"].toInt()] = uid_entry.toObject()["instance_uid"].toInt();
            }

            for (const auto& gid_entry : entry.toObject()["gid_mappings"].toArray())
            {
                gid_map[gid_entry.toObject()["host_gid"].toInt()] = gid_entry.toObject()["instance_gid"].toInt();
            }

            mp::VMMount mount{source_path, gid_map, uid_map};
            mounts[target_path] = mount;
        }

        reconstructed_records[key] = {num_cores,
                                      mp::MemorySize{mem_size.empty() ? mp::default_memory_size : mem_size},
                                      mp::MemorySize{disk_space.empty() ? mp::default_disk_size : disk_space},
                                      mac_addr,
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

    mp::Query query;
    query.name = name;

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

auto validate_create_arguments(const mp::LaunchRequest* request)
{
    static const auto min_mem = try_mem_size(mp::min_memory_size);
    static const auto min_disk = try_mem_size(mp::min_disk_size);
    assert(min_mem && min_disk);

    auto mem_size_str = request->mem_size();
    auto disk_space_str = request->disk_space();
    auto instance_name = request->instance_name();
    auto option_errors = mp::LaunchError{};

    const auto opt_mem_size = try_mem_size(mem_size_str.empty() ? mp::default_memory_size : mem_size_str);
    const auto opt_disk_space = try_mem_size(disk_space_str.empty() ? mp::default_disk_size : disk_space_str);

    mp::MemorySize mem_size{}, disk_space{};
    if (opt_mem_size && *opt_mem_size >= min_mem)
        mem_size = *opt_mem_size;
    else
        option_errors.add_error_codes(mp::LaunchError::INVALID_MEM_SIZE);

    if (opt_disk_space && *opt_disk_space >= min_disk)
        disk_space = *opt_disk_space;
    else
        option_errors.add_error_codes(mp::LaunchError::INVALID_DISK_SIZE);

    if (!request->instance_name().empty() && !mp::utils::valid_hostname(request->instance_name()))
        option_errors.add_error_codes(mp::LaunchError::INVALID_HOSTNAME);

    struct CheckedArguments
    {
        mp::MemorySize mem_size;
        mp::MemorySize disk_space;
        std::string instance_name;
        mp::LaunchError option_errors;
    } ret{mem_size, disk_space, instance_name, option_errors};
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

auto get_unique_id(const mp::Path& data_path)
{
    QFile id_file{QDir(data_path).filePath(uuid_file_name)};
    QString id;

    if (!id_file.exists())
    {
        id = mp::utils::make_uuid();
        id_file.open(QIODevice::WriteOnly);
        id_file.write(id.toUtf8());
    }
    else
    {
        id_file.open(QIODevice::ReadOnly);
        id = QString(id_file.readAll());
    }

    id_file.close();
    return id;
}

void persist_metrics_opt_in_data(const mp::MetricsOptInData& opt_in_data, const mp::Path& data_path)
{
    YAML::Node opt_in;
    opt_in["status"] = static_cast<int>(opt_in_data.opt_in_status);
    opt_in["delay_count"] = opt_in_data.delay_opt_in_count;

    YAML::Emitter emitter;
    emitter << opt_in << YAML::Newline;

    QFile opt_in_file{QDir(data_path).filePath(metrics_opt_in_file)};
    opt_in_file.open(QIODevice::WriteOnly);
    opt_in_file.write(emitter.c_str());
}

auto get_metrics_opt_in(const mp::Path& data_path)
{
    YAML::Node config;
    try
    {
        config = YAML::LoadFile(QDir(data_path).filePath(metrics_opt_in_file).toStdString());
    }
    catch (const std::exception& e)
    {
        // Ignore exceptions in this case
    }

    mp::MetricsOptInData opt_in_data;

    if (config.IsNull())
    {
        opt_in_data.opt_in_status = mp::OptInStatus::UNKNOWN;
        opt_in_data.delay_opt_in_count = 0;

        persist_metrics_opt_in_data(opt_in_data, data_path);
    }
    else
    {
        opt_in_data.opt_in_status = static_cast<mp::OptInStatus::Status>(config["status"].as<int>());
        opt_in_data.delay_opt_in_count = config["delay_count"].as<int>();
    }

    return opt_in_data;
}

auto connect_rpc(mp::DaemonRpc& rpc, mp::Daemon& daemon)
{
    QObject::connect(&rpc, &mp::DaemonRpc::on_create, &daemon, &mp::Daemon::create);
    QObject::connect(&rpc, &mp::DaemonRpc::on_launch, &daemon, &mp::Daemon::launch);
    QObject::connect(&rpc, &mp::DaemonRpc::on_purge, &daemon, &mp::Daemon::purge);
    QObject::connect(&rpc, &mp::DaemonRpc::on_find, &daemon, &mp::Daemon::find);
    QObject::connect(&rpc, &mp::DaemonRpc::on_info, &daemon, &mp::Daemon::info);
    QObject::connect(&rpc, &mp::DaemonRpc::on_list, &daemon, &mp::Daemon::list);
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

mp::SSHProcess exec_and_log(mp::SSHSession& session, const std::string& cmd)
{
    mpl::log(mpl::Level::debug, category, fmt::format("Executing {}.", cmd));
    return session.exec(cmd);
}

grpc::Status stop_accepting_ssh_connections(mp::SSHSession& session)
{
    auto proc = exec_and_log(session, stop_ssh_cmd);
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

    auto proc = exec_and_log(session, reboot_cmd);
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

QStringList filter_unsupported_aliases(const QStringList& aliases, const std::string& remote)
{
    QStringList supported_aliases;

    for (const auto& alias : aliases)
    {
        if (mp::platform::is_alias_supported(alias.toStdString(), remote))
        {
            supported_aliases.append(alias);
        }
    }
    return supported_aliases;
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
} // namespace

mp::Daemon::Daemon(std::unique_ptr<const DaemonConfig> the_config)
    : config{std::move(the_config)},
      vm_instance_specs{load_db(
          mp::utils::backend_directory_path(config->data_directory, config->factory->get_backend_directory_name()),
          mp::utils::backend_directory_path(config->cache_directory, config->factory->get_backend_directory_name()))},
      daemon_rpc{config->server_address, config->connection_type, *config->cert_provider, *config->client_cert_store},
      metrics_provider{"https://api.jujucharms.com/omnibus/v4/multipass/metrics", get_unique_id(config->data_directory),
                       config->data_directory},
      metrics_opt_in{get_metrics_opt_in(config->data_directory)},
      instance_mounts{*config->ssh_key_provider}
{
    connect_rpc(daemon_rpc, *this);
    std::vector<std::string> invalid_specs;
    bool mac_addr_missing{false};
    for (auto& entry : vm_instance_specs)
    {
        const auto& name = entry.first;
        auto& spec = entry.second;

        if (!config->vault->has_record_for(name))
        {
            invalid_specs.push_back(name);
            continue;
        }

        auto mac_addr = spec.mac_addr;
        if (mac_addr.empty())
        {
            mac_addr = mp::utils::generate_mac_address();
            vm_instance_specs[name].mac_addr = mac_addr;
            mac_addr_missing = true;
        }
        allocated_mac_addrs.insert(mac_addr);

        auto vm_image = fetch_image_for(name, config->factory->fetch_type(), *config->vault);
        const auto instance_dir = mp::utils::base_dir(vm_image.image_path);
        const auto cloud_init_iso = instance_dir.filePath("cloud-init-config.iso");
        mp::VirtualMachineDescription vm_desc{spec.num_cores, spec.mem_size,     spec.disk_space, name,
                                              mac_addr,       spec.ssh_username, vm_image,        cloud_init_iso};

        try
        {
            auto& instance_record = spec.deleted ? deleted_instances : vm_instances;
            instance_record[name] = config->factory->create_virtual_machine(vm_desc, *this);
        }
        catch (const std::exception& e)
        {
            mpl::log(mpl::Level::error, category, fmt::format("Removing instance {}: {}", name, e.what()));
            invalid_specs.push_back(name);
            config->vault->remove(name);
        }

        // FIXME: somehow we're writing contradictory state to disk.
        if (spec.deleted && spec.state != VirtualMachine::State::stopped)
        {
            mpl::log(mpl::Level::warning, category,
                     fmt::format("{} is deleted but has incompatible state {}, reseting state to 0 (stopped)", name,
                                 static_cast<int>(spec.state)));
            spec.state = VirtualMachine::State::stopped;
        }

        if (spec.state == VirtualMachine::State::running && vm_instances[name]->state != VirtualMachine::State::running)
        {
            assert(!spec.deleted);
            mpl::log(mpl::Level::info, category, fmt::format("{} needs starting. Starting now...", name));

            QTimer::singleShot(0, [this, &name] {
                vm_instances[name]->start();
                on_restart(name);
            });
        }
    }

    for (const auto& bad_spec : invalid_specs)
    {
        vm_instance_specs.erase(bad_spec);
    }

    if (!invalid_specs.empty() || mac_addr_missing)
        persist_instances();

    for (const auto& image_host : config->image_hosts)
    {
        for (const auto& remote : image_host->supported_remotes())
        {
            remote_image_host_map[remote] = image_host.get();
        }
    }

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

void mp::Daemon::create(const CreateRequest* request, grpc::ServerWriter<CreateReply>* server,
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

void mp::Daemon::launch(const LaunchRequest* request, grpc::ServerWriter<LaunchReply>* server,
                        std::promise<grpc::Status>* status_promise) // clang-format off
try // clang-format on
{
    mpl::ClientLogger<LaunchReply> logger{mpl::level_from(request->verbosity_level()), *config->logger, server};
    if (metrics_opt_in.opt_in_status == OptInStatus::UNKNOWN || metrics_opt_in.opt_in_status == OptInStatus::LATER)
    {
        if (++metrics_opt_in.delay_opt_in_count % 3 == 0)
        {
            metrics_opt_in.opt_in_status = OptInStatus::PENDING;
            persist_metrics_opt_in_data(metrics_opt_in, config->data_directory);

            LaunchReply reply;
            reply.set_metrics_pending(true);
            server->Write(reply);

            return status_promise->set_value(grpc::Status::OK);
        }

        persist_metrics_opt_in_data(metrics_opt_in, config->data_directory);
    }
    else if (metrics_opt_in.opt_in_status == OptInStatus::PENDING)
    {
        if (request->opt_in_reply().opt_in_status() != OptInStatus::UNKNOWN)
        {
            metrics_opt_in.opt_in_status = request->opt_in_reply().opt_in_status();
            persist_metrics_opt_in_data(metrics_opt_in, config->data_directory);

            if (metrics_opt_in.opt_in_status == OptInStatus::DENIED)
                metrics_provider.send_denied();
        }
    }

    if (metrics_opt_in.opt_in_status == OptInStatus::ACCEPTED)
        metrics_provider.send_metrics();

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

void mp::Daemon::purge(const PurgeRequest* request, grpc::ServerWriter<PurgeReply>* server,
                       std::promise<grpc::Status>* status_promise) // clang-format off
try // clang-format on
{
    for (const auto& del : deleted_instances)
        release_resources(del.first);

    deleted_instances.clear();
    persist_instances();

    status_promise->set_value(grpc::Status::OK);
}
catch (const std::exception& e)
{
    status_promise->set_value(grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, e.what(), ""));
}

void mp::Daemon::find(const FindRequest* request, grpc::ServerWriter<FindReply>* server,
                      std::promise<grpc::Status>* status_promise) // clang-format off
try // clang-format on
{
    mpl::ClientLogger<FindReply> logger{mpl::level_from(request->verbosity_level()), *config->logger, server};
    FindReply response;

    if (!request->search_string().empty())
    {
        std::vector<VMImageInfo> vm_images_info;
        auto remote{request->remote_name()};

        if (!remote.empty())
        {
            auto it = remote_image_host_map.find(remote);
            if (it == remote_image_host_map.end())
                throw std::runtime_error(fmt::format("Remote \"{}\" is unknown.", remote));

            if (!mp::platform::is_remote_supported(remote))
                throw std::runtime_error(fmt::format(
                    "{} is not a supported remote. Please use `multipass find` for list of supported images.", remote));

            auto images_info = it->second->all_info_for(
                {"", request->search_string(), false, remote, Query::Type::Alias, request->allow_unsupported()});

            if (!images_info.empty())
            {
                vm_images_info = std::move(images_info);
            }
        }
        else
        {
            for (const auto& image_host : config->image_hosts)
            {
                auto images_info = image_host->all_info_for(
                    {"", request->search_string(), false, remote, Query::Type::Alias, request->allow_unsupported()});

                if (!images_info.empty())
                {
                    vm_images_info = std::move(images_info);
                    break;
                }
            }
        }

        if (vm_images_info.empty())
            throw std::runtime_error(fmt::format("Unable to find an image matching \"{}\"", request->search_string()));

        if (!mp::platform::is_alias_supported(request->search_string(), remote))
            throw std::runtime_error(
                fmt::format("{} is not a supported alias. Please use `multipass find` for supported image aliases.",
                            request->search_string()));

        for (const auto& info : vm_images_info)
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
            alias_entry->set_remote_name(remote);
            alias_entry->set_alias(name);
        }
    }
    else if (!request->remote_name().empty())
    {
        const auto remote = request->remote_name();

        if (!mp::platform::is_remote_supported(remote))
            throw std::runtime_error(fmt::format(
                "{} is not a supported remote. Please use `multipass find` for list of supported images.", remote));

        auto it = remote_image_host_map.find(remote);
        if (it == remote_image_host_map.end())
            throw std::runtime_error(fmt::format("Remote \"{}\" is unknown.", remote));

        auto vm_images_info = it->second->all_images_for(remote, request->allow_unsupported());
        for (const auto& info : vm_images_info)
        {
            if (!info.aliases.empty())
            {
                auto entry = response.add_images_info();
                for (const auto& alias : info.aliases)
                {
                    if (!mp::platform::is_alias_supported(alias.toStdString(), remote))
                        continue;

                    auto alias_entry = entry->add_aliases_info();
                    alias_entry->set_remote_name(request->remote_name());
                    alias_entry->set_alias(alias.toStdString());
                }

                entry->set_os(info.os.toStdString());
                entry->set_release(info.release_title.toStdString());
                entry->set_version(info.version.toStdString());
            }
        }
    }
    else
    {
        for (const auto& image_host : config->image_hosts)
        {
            std::unordered_set<std::string> image_found;
            const auto default_remote{"release"};
            auto action = [&response, &image_found, default_remote, request](const std::string& remote,
                                                                             const mp::VMImageInfo& info) {
                if (!mp::platform::is_remote_supported(remote))
                    return;

                if (info.supported || request->allow_unsupported())
                {
                    if (image_found.find(info.release_title.toStdString()) == image_found.end())
                    {
                        const auto supported_aliases = filter_unsupported_aliases(info.aliases, remote);
                        if (!supported_aliases.empty())
                        {
                            auto entry = response.add_images_info();
                            for (const auto& alias : supported_aliases)
                            {
                                auto alias_entry = entry->add_aliases_info();
                                if (remote != default_remote)
                                    alias_entry->set_remote_name(remote);
                                alias_entry->set_alias(alias.toStdString());
                            }

                            image_found.insert(info.release_title.toStdString());
                            entry->set_os(info.os.toStdString());
                            entry->set_release(info.release_title.toStdString());
                            entry->set_version(info.version.toStdString());
                        }
                    }
                }
            };

            image_host->for_each_entry_do(action);
        }
    }
    server->Write(response);
    status_promise->set_value(grpc::Status::OK);
}
catch (const std::exception& e)
{
    status_promise->set_value(grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, e.what(), ""));
}

void mp::Daemon::info(const InfoRequest* request, grpc::ServerWriter<InfoReply>* server,
                      std::promise<grpc::Status>* status_promise) // clang-format off
try // clang-format on
{
    mpl::ClientLogger<InfoReply> logger{mpl::level_from(request->verbosity_level()), *config->logger, server};
    InfoReply response;

    fmt::memory_buffer errors;
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

        for (const auto& mount : vm_specs.mounts)
        {
            if (mount.second.source_path.size() > mount_info->longest_path_len())
            {
                mount_info->set_longest_path_len(mount.second.source_path.size());
            }

            auto entry = mount_info->add_mount_paths();
            entry->set_source_path(mount.second.source_path);
            entry->set_target_path(mount.first);

            for (const auto uid_map : mount.second.uid_map)
            {
                (*entry->mutable_mount_maps()->mutable_uid_map())[uid_map.first] = uid_map.second;
            }
            for (const auto gid_map : mount.second.gid_map)
            {
                (*entry->mutable_mount_maps()->mutable_gid_map())[gid_map.first] = gid_map.second;
            }
        }

        if (mp::utils::is_running(present_state))
        {
            mp::SSHSession session{vm->ssh_hostname(), vm->ssh_port(), vm_specs.ssh_username,
                                   *config->ssh_key_provider};

            auto run_in_vm = [&session](const std::string& cmd) {
                auto proc = session.exec(cmd);
                if (proc.exit_code() != 0)
                {
                    auto error_msg = proc.read_std_error();
                    mpl::log(
                        mpl::Level::warning, category,
                        fmt::format("failed to run '{}', error message: '{}'", cmd, mp::utils::trim_end(error_msg)));
                    return std::string{};
                }

                auto output = proc.read_std_output();
                if (output.empty())
                {
                    mpl::log(mpl::Level::warning, category, fmt::format("no output after running '{}'", cmd));
                    return std::string{};
                }

                return mp::utils::trim_end(output);
            };

            info->set_load(run_in_vm("cat /proc/loadavg | cut -d ' ' -f1-3"));
            info->set_memory_usage(run_in_vm("free -b | sed '1d;3d' | awk '{printf $3}'"));
            info->set_memory_total(run_in_vm("free -b | sed '1d;3d' | awk '{printf $2}'"));
            info->set_disk_usage(
                run_in_vm("df --output=used `awk '$2 == \"/\" { print $1 }' /proc/mounts` -B1 | sed 1d"));
            info->set_disk_total(
                run_in_vm("df --output=size `awk '$2 == \"/\" { print $1 }' /proc/mounts` -B1 | sed 1d"));
            info->set_ipv4(vm->ipv4());

            auto current_release = run_in_vm("lsb_release -ds");
            info->set_current_release(!current_release.empty() ? current_release : original_release);
        }
    }

    auto status = grpc_status_for(errors);
    if (status.ok())
        server->Write(response);

    status_promise->set_value(status);
}
catch (const std::exception& e)
{
    status_promise->set_value(grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, e.what(), ""));
}

void mp::Daemon::list(const ListRequest* request, grpc::ServerWriter<ListReply>* server,
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

        if (mp::utils::is_running(present_state))
            entry->set_ipv4(vm->ipv4());
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

void mp::Daemon::mount(const MountRequest* request, grpc::ServerWriter<MountReply>* server,
                       std::promise<grpc::Status>* status_promise) // clang-format off
try // clang-format on
{
    mpl::ClientLogger<MountReply> logger{mpl::level_from(request->verbosity_level()), *config->logger, server};

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

    std::unordered_map<int, int> uid_map{request->mount_maps().uid_map().begin(),
                                         request->mount_maps().uid_map().end()};
    std::unordered_map<int, int> gid_map{request->mount_maps().gid_map().begin(),
                                         request->mount_maps().gid_map().end()};

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
            // If the given path is relative, we need to resolve the fill
            // absolute path. For this, we execute the command 'pwd'.
            if (QDir(QString::fromStdString(target_path)).isRelative())
            {
                mp::SSHSession session{vm->ssh_hostname(), vm->ssh_port(), vm_specs.ssh_username, *config->ssh_key_provider};

                std::string home;

                auto proc = session.exec("pwd");
                if (proc.exit_code() != 0)
                {
                    auto error_msg = proc.read_std_error();
                    mpl::log(
                        mpl::Level::warning, category,
                        fmt::format("failed to run 'pwd', error message: '{}'", mp::utils::trim_end(error_msg)));
                    home = std::string{};
                }

                home = proc.read_std_output();
                mp::utils::trim_end(home);

                target_path = mp::utils::normalize_absolute_path(home + '/' + target_path);
                mpl::log(mpl::Level::info, category, fmt::format("home directory: {}, target_path: {}", home, target_path));
            }
            else
            {
                target_path = mp::utils::normalize_absolute_path(target_path);
                mpl::log(mpl::Level::info, category, fmt::format("target_path: {}", target_path));
            }

            try
            {
                instance_mounts.start_mount(vm.get(), request->source_path(), target_path, gid_map, uid_map);
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
                    install_sshfs(vm.get(), name);
                    instance_mounts.start_mount(vm.get(), request->source_path(), target_path, gid_map, uid_map);
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

        VMMount mount{request->source_path(), gid_map, uid_map};
        vm_specs.mounts[target_path] = mount;
    }

    persist_instances();

    status_promise->set_value(grpc_status_for(errors));
}
catch (const std::exception& e)
{
    status_promise->set_value(grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, e.what(), ""));
}

void mp::Daemon::recover(const RecoverRequest* request, grpc::ServerWriter<RecoverReply>* server,
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

void mp::Daemon::ssh_info(const SSHInfoRequest* request, grpc::ServerWriter<SSHInfoReply>* server,
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

void mp::Daemon::start(const StartRequest* request, grpc::ServerWriter<StartReply>* server,
                       std::promise<grpc::Status>* status_promise) // clang-format off
try // clang-format on
{
    mpl::ClientLogger<StartReply> logger{mpl::level_from(request->verbosity_level()), *config->logger, server};

    if (!instances_running(vm_instances))
        config->factory->hypervisor_health_check();

    mp::StartError start_error;
    auto* errors = start_error.mutable_instance_errors();

    std::vector<decltype(vm_instances)::key_type> vms;
    for (const auto& name : request->instance_names().instance_name())
    {
        auto it = vm_instances.find(name);
        if (it == vm_instances.end())
            errors->insert({name, deleted_instances.find(name) == deleted_instances.end()
                                      ? mp::StartError::DOES_NOT_EXIST
                                      : mp::StartError::INSTANCE_DELETED});
        else if (it->second->current_state() == VirtualMachine::State::delayed_shutdown)
            delayed_shutdown_instances.erase(name);
        else if (it->second->current_state() != VirtualMachine::State::running)
            vms.push_back(name);
    }

    if (start_error.instance_errors_size())
        return status_promise->set_value(
            grpc::Status(grpc::StatusCode::ABORTED, "instance(s) missing", start_error.SerializeAsString()));

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
        QtConcurrent::run(this, &Daemon::async_wait_for_ready_all<StartReply>, server, vms, status_promise));
}
catch (const std::exception& e)
{
    status_promise->set_value(grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, e.what(), ""));
}

void mp::Daemon::stop(const StopRequest* request, grpc::ServerWriter<StopReply>* server,
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

void mp::Daemon::suspend(const SuspendRequest* request, grpc::ServerWriter<SuspendReply>* server,
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
            instance_mounts.stop_all_mounts_for_instance(vm.vm_name);
            vm.suspend();
            return grpc::Status::OK;
        });
    }

    status_promise->set_value(status);
}
catch (const std::exception& e)
{
    status_promise->set_value(grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, e.what(), ""));
}

void mp::Daemon::restart(const RestartRequest* request, grpc::ServerWriter<RestartReply>* server,
                         std::promise<grpc::Status>* status_promise) // clang-format off
try // clang-format on
{
    mpl::ClientLogger<RestartReply> logger{mpl::level_from(request->verbosity_level()), *config->logger, server};

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
    future_watcher->setFuture(
        QtConcurrent::run(this, &Daemon::async_wait_for_ready_all<RestartReply>, server, instances, status_promise));
}
catch (const std::exception& e)
{
    status_promise->set_value(grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, e.what(), ""));
}

void mp::Daemon::delet(const DeleteRequest* request, grpc::ServerWriter<DeleteReply>* server,
                       std::promise<grpc::Status>* status_promise) // clang-format off
try // clang-format on
{
    mpl::ClientLogger<DeleteReply> logger{mpl::level_from(request->verbosity_level()), *config->logger, server};

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
                release_resources(name);
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

void mp::Daemon::umount(const UmountRequest* request, grpc::ServerWriter<UmountReply>* server,
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
        auto& vm_specs = vm_instance_specs[name];

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
                // If the given path is relative, we need to resolve the fill
                // absolute path. For this, we execute the command 'pwd'.
                if (QDir(QString::fromStdString(target_path)).isRelative())
                {
                    mp::SSHSession session{vm->ssh_hostname(), vm->ssh_port(), vm_specs.ssh_username, *config->ssh_key_provider};

                    std::string home;

                    auto proc = session.exec("pwd");
                    if (proc.exit_code() != 0)
                    {
                        auto error_msg = proc.read_std_error();
                        mpl::log(
                            mpl::Level::warning, category,
                            fmt::format("failed to run 'pwd', error message: '{}'", mp::utils::trim_end(error_msg)));
                        home = std::string{};
                    }

                    home = proc.read_std_output();
                    mp::utils::trim_end(home);

                    target_path = mp::utils::normalize_absolute_path(home + '/' + target_path);
                    mpl::log(mpl::Level::info, category, fmt::format("home directory: {}, target_path: {}", home, target_path));
                }
                else
                {
                    target_path = mp::utils::normalize_absolute_path(target_path);
                    mpl::log(mpl::Level::info, category, fmt::format("target_path: {}", target_path));
                }

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

void mp::Daemon::version(const VersionRequest* request, grpc::ServerWriter<VersionReply>* server,
                         std::promise<grpc::Status>* status_promise)
{
    mpl::ClientLogger<VersionReply> logger{mpl::level_from(request->verbosity_level()), *config->logger, server};

    VersionReply reply;
    reply.set_version(multipass::version_string);
    config->update_prompt->populate(reply.mutable_update_info());
    server->Write(reply);
    status_promise->set_value(grpc::Status::OK);
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
                                                std::vector<std::string>{name}, nullptr));
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

void mp::Daemon::persist_instances()
{
    auto vm_spec_to_json = [](const mp::VMSpecs& specs) -> QJsonObject {
        QJsonObject json;
        json.insert("num_cores", specs.num_cores);
        json.insert("mem_size", QString::number(specs.mem_size.in_bytes()));
        json.insert("disk_space", QString::number(specs.disk_space.in_bytes()));
        json.insert("mac_addr", QString::fromStdString(specs.mac_addr));
        json.insert("ssh_username", QString::fromStdString(specs.ssh_username));
        json.insert("state", static_cast<int>(specs.state));
        json.insert("deleted", specs.deleted);
        json.insert("metadata", specs.metadata);

        QJsonArray mounts;
        for (const auto& mount : specs.mounts)
        {
            QJsonObject entry;
            entry.insert("source_path", QString::fromStdString(mount.second.source_path));
            entry.insert("target_path", QString::fromStdString(mount.first));

            QJsonArray uid_map;
            for (const auto& map : mount.second.uid_map)
            {
                QJsonObject map_entry;
                map_entry.insert("host_uid", map.first);
                map_entry.insert("instance_uid", map.second);

                uid_map.append(map_entry);
            }

            entry.insert("uid_mappings", uid_map);

            QJsonArray gid_map;
            for (const auto& map : mount.second.gid_map)
            {
                QJsonObject map_entry;
                map_entry.insert("host_gid", map.first);
                map_entry.insert("instance_gid", map.second);

                gid_map.append(map_entry);
            }

            entry.insert("gid_mappings", gid_map);
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
    vm_instance_specs.erase(instance);
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

void mp::Daemon::create_vm(const CreateRequest* request, grpc::ServerWriter<CreateReply>* server,
                           std::promise<grpc::Status>* status_promise, bool start)
{
    auto checked_args = validate_create_arguments(request);

    if (!checked_args.option_errors.error_codes().empty())
    {
        return status_promise->set_value(grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Invalid arguments supplied",
                                                      checked_args.option_errors.SerializeAsString()));
    }

    auto name = name_from(checked_args.instance_name, *config->name_generator, vm_instances);

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

    preparing_instances.insert(name);

    auto prepare_future_watcher = new QFutureWatcher<VirtualMachineDescription>();

    QObject::connect(
        prepare_future_watcher, &QFutureWatcher<VirtualMachineDescription>::finished,
        [this, server, status_promise, name, start, prepare_future_watcher] {
            try
            {
                auto vm_desc = prepare_future_watcher->future().result();

                vm_instances[name] = config->factory->create_virtual_machine(vm_desc, *this);
                vm_instance_specs[name] = {vm_desc.num_cores,
                                           vm_desc.mem_size,
                                           vm_desc.disk_space,
                                           vm_desc.mac_addr,
                                           config->ssh_username,
                                           VirtualMachine::State::off,
                                           {},
                                           false,
                                           QJsonObject()};
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
                                                                server, std::vector<std::string>{name},
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

    prepare_future_watcher->setFuture(
        QtConcurrent::run([this, server, request, name, checked_args]() -> VirtualMachineDescription {
            try
            {
                auto query = query_from(request, name);

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

                CreateReply reply;
                reply.set_create_message("Creating " + name);
                server->Write(reply);
                auto vm_image = config->vault->fetch_image(fetch_type, query, prepare_action, progress_monitor);

                reply.set_create_message("Configuring " + name);
                server->Write(reply);
                auto vendor_data_cloud_init_config =
                    make_cloud_init_vendor_config(*config->ssh_key_provider, request->time_zone(), config->ssh_username,
                                                  config->factory->get_backend_version_string().toStdString());
                auto meta_data_cloud_init_config = make_cloud_init_meta_config(name);
                auto user_data_cloud_init_config = YAML::Load(request->cloud_init_user_data());
                prepare_user_data(user_data_cloud_init_config, vendor_data_cloud_init_config);
                config->factory->configure(name, meta_data_cloud_init_config, vendor_data_cloud_init_config);

                std::string mac_addr;
                while (true)
                {
                    mac_addr = mp::utils::generate_mac_address();

                    auto it = allocated_mac_addrs.find(mac_addr);
                    if (it == allocated_mac_addrs.end())
                    {
                        allocated_mac_addrs.insert(mac_addr);
                        break;
                    }
                }
                auto vm_desc = to_machine_desc(request, name, checked_args.mem_size, checked_args.disk_space, mac_addr,
                                               config->ssh_username, vm_image, meta_data_cloud_init_config,
                                               user_data_cloud_init_config, vendor_data_cloud_init_config);

                config->factory->prepare_instance_image(vm_image, vm_desc);

                return vm_desc;
            }
            catch (const std::exception& e)
            {
                throw CreateImageException(e.what());
            }
        }));
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

void mp::Daemon::install_sshfs(VirtualMachine* vm, const std::string& name)
{
    auto& key_provider = *config->ssh_key_provider;

    SSHSession session{vm->ssh_hostname(), vm->ssh_port(), vm->ssh_username(), key_provider};

    mpl::log(mpl::Level::info, category, fmt::format("Installing sshfs in \'{}\'", name));

    int retries{0};
    while (++retries <= max_install_sshfs_retries)
    {
        try
        {
            auto proc = session.exec("sudo apt update && sudo apt install -y sshfs");
            if (proc.exit_code(std::chrono::minutes(5)) != 0)
            {
                auto error_msg = proc.read_std_error();
                mpl::log(mpl::Level::warning, category,
                         fmt::format("Failed to install 'sshfs', error message: '{}'", mp::utils::trim_end(error_msg)));
            }
            else
            {
                break;
            }
        }
        catch (const mp::ExitlessSSHProcessException&)
        {
            mpl::log(mpl::Level::info, category, fmt::format("Timeout while installing 'sshfs' in '{}'", name));
        }
    }

    if (retries > max_install_sshfs_retries)
        throw mp::SSHFSMissingError();
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
                                                                 grpc::ServerWriter<Reply>* server)
{
    fmt::memory_buffer errors;
    try
    {
        auto it = vm_instances.find(name);
        auto vm = it->second;
        vm->wait_until_ssh_up(up_timeout);

        if (std::is_same<Reply, LaunchReply>::value)
        {
            if (server)
            {
                Reply reply;
                reply.set_reply_message("Waiting for initialization to complete");
                server->Write(reply);
            }

            mp::utils::wait_for_cloud_init(vm.get(), cloud_init_timeout, *config->ssh_key_provider);
        }

        std::vector<std::string> invalid_mounts;
        auto& mounts = vm_instance_specs[name].mounts;
        for (const auto& mount_entry : mounts)
        {
            auto& target_path = mount_entry.first;
            auto& source_path = mount_entry.second.source_path;
            auto& uid_map = mount_entry.second.uid_map;
            auto& gid_map = mount_entry.second.gid_map;

            try
            {
                instance_mounts.start_mount(vm.get(), source_path, target_path, gid_map, uid_map);
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

                    install_sshfs(vm.get(), name);
                    instance_mounts.start_mount(vm.get(), source_path, target_path, gid_map, uid_map);
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
    catch (const std::exception& e)
    {
        fmt::format_to(errors, e.what());
    }

    return fmt::to_string(errors);
}

template <typename Reply>
mp::Daemon::AsyncOperationStatus mp::Daemon::async_wait_for_ready_all(grpc::ServerWriter<Reply>* server,
                                                                      const std::vector<std::string>& vms,
                                                                      std::promise<grpc::Status>* status_promise)
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
                auto future =
                    QtConcurrent::run(this, &Daemon::async_wait_for_ssh_and_start_mounts_for<Reply>, name, server);
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
