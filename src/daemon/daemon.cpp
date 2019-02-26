/*
 * Copyright (C) 2017-2019 Canonical, Ltd.
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
#include <multipass/exceptions/exitless_sshprocess_exception.h>
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

#include <fmt/format.h>
#include <yaml-cpp/yaml.h>

#include <QDir>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

#include <cassert>
#include <functional>
#include <stdexcept>
#include <unordered_set>
#include <utility>

namespace mp = multipass;
namespace mpl = multipass::logging;

namespace
{

using namespace std::chrono_literals;

constexpr auto category = "daemon";
constexpr auto instance_db_name = "multipassd-vm-instances.json";
constexpr auto uuid_file_name = "multipass-unique-id";
constexpr auto metrics_opt_in_file = "multipassd-send-metrics.yaml";
constexpr auto reboot_cmd = "sudo reboot";
constexpr auto up_timeout = 2min; // This may be tweaked as appropriate and used in places that wait for ssh to be up
constexpr auto stop_ssh_cmd = "sudo systemctl stop ssh";
const auto normalized_min_mem = mp::utils::in_bytes(mp::min_memory_size);
const auto normalized_min_disk = mp::utils::in_bytes(mp::min_disk_size);

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

    return {name, image, false, request->remote_name(), query_type};
}

auto make_cloud_init_vendor_config(const mp::SSHKeyProvider& key_provider, const std::string& time_zone,
                                   const std::string& username)
{
    auto ssh_key_line = fmt::format("ssh-rsa {} multipass@localhost", key_provider.public_key_as_base64());

    auto config = YAML::Load(mp::base_cloud_init_config);
    config["ssh_authorized_keys"].push_back(ssh_key_line);
    config["timezone"] = time_zone;
    config["system_info"]["default_user"]["name"] = username;

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

    auto packages = user_data_config["packages"];
    if (packages.IsSequence())
        packages.push_back("sshfs");

    auto keys = user_data_config["ssh_authorized_keys"];
    if (keys.IsSequence())
        keys.push_back(vendor_config["ssh_authorized_keys"][0]);
}

mp::VirtualMachineDescription to_machine_desc(const mp::LaunchRequest* request, const std::string& name,
                                              const std::string& mem_size, const std::string& disk_space,
                                              const std::string& mac_addr, const std::string& ssh_username,
                                              const mp::VMImage& image, YAML::Node& meta_data_config,
                                              YAML::Node& user_data_config, YAML::Node& vendor_data_config,
                                              const mp::SSHKeyProvider& key_provider)
{
    const auto num_cores = request->num_cores() < 1 ? 1 : request->num_cores();
    const auto instance_dir = mp::utils::base_dir(image.image_path);
    const auto cloud_init_iso =
        make_cloud_init_image(name, instance_dir, meta_data_config, user_data_config, vendor_data_config);
    return {num_cores, mem_size, disk_space, name, mac_addr, ssh_username, image, cloud_init_iso, key_provider};
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
        auto mem_size = record["mem_size"].toString();
        auto disk_space = record["disk_space"].toString();
        auto mac_addr = record["mac_addr"].toString();
        auto ssh_username = record["ssh_username"].toString();
        auto state = record["state"].toInt();
        auto deleted = record["deleted"].toBool();
        auto metadata = record["metadata"].toObject();

        if (ssh_username.isEmpty())
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
                                      mem_size.toStdString(),
                                      disk_space.toStdString(),
                                      mac_addr.toStdString(),
                                      ssh_username.toStdString(),
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

auto validate_create_arguments(const mp::LaunchRequest* request)
{
    auto mem_size = request->mem_size();
    auto disk_space = request->disk_space();
    auto instance_name = request->instance_name();
    auto option_errors = mp::LaunchError{};

    const auto opt_mem_size = mp::utils::in_bytes(mem_size.empty() ? "1G" : mem_size);
    const auto opt_disk_space = mp::utils::in_bytes(disk_space.empty() ? "5G" : disk_space);

    if(opt_mem_size && *opt_mem_size >= normalized_min_mem)
        mem_size = mp::utils::in_bytes_string(*opt_mem_size);
    else
        option_errors.add_error_codes(mp::LaunchError::INVALID_MEM_SIZE);

    if(opt_disk_space && *opt_disk_space >= normalized_min_disk)
        disk_space = mp::utils::in_bytes_string(*opt_disk_space);
    else
        option_errors.add_error_codes(mp::LaunchError::INVALID_DISK_SIZE);

    if(!request->instance_name().empty() &&
       !mp::utils::valid_hostname(request->instance_name()))
        option_errors.add_error_codes(mp::LaunchError::INVALID_HOSTNAME);

    return make_tuple(mem_size, disk_space, instance_name, option_errors);
}

auto grpc_status_for_mount_error(const std::string& instance_name)
{
    mp::MountError mount_error;
    mount_error.set_error_code(mp::MountError::SSHFS_MISSING);
    mount_error.set_instance_name(instance_name);

    return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, "Mount failed", mount_error.SerializeAsString());
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
    QObject::connect(&rpc, &mp::DaemonRpc::on_launch, &daemon, &mp::Daemon::launch, Qt::BlockingQueuedConnection);
    QObject::connect(&rpc, &mp::DaemonRpc::on_purge, &daemon, &mp::Daemon::purge, Qt::BlockingQueuedConnection);
    QObject::connect(&rpc, &mp::DaemonRpc::on_find, &daemon, &mp::Daemon::find, Qt::BlockingQueuedConnection);
    QObject::connect(&rpc, &mp::DaemonRpc::on_info, &daemon, &mp::Daemon::info, Qt::BlockingQueuedConnection);
    QObject::connect(&rpc, &mp::DaemonRpc::on_list, &daemon, &mp::Daemon::list, Qt::BlockingQueuedConnection);
    QObject::connect(&rpc, &mp::DaemonRpc::on_mount, &daemon, &mp::Daemon::mount, Qt::BlockingQueuedConnection);
    QObject::connect(&rpc, &mp::DaemonRpc::on_recover, &daemon, &mp::Daemon::recover, Qt::BlockingQueuedConnection);
    QObject::connect(&rpc, &mp::DaemonRpc::on_ssh_info, &daemon, &mp::Daemon::ssh_info, Qt::BlockingQueuedConnection);
    QObject::connect(&rpc, &mp::DaemonRpc::on_start, &daemon, &mp::Daemon::start, Qt::BlockingQueuedConnection);
    QObject::connect(&rpc, &mp::DaemonRpc::on_stop, &daemon, &mp::Daemon::stop, Qt::BlockingQueuedConnection);
    QObject::connect(&rpc, &mp::DaemonRpc::on_suspend, &daemon, &mp::Daemon::suspend, Qt::BlockingQueuedConnection);
    QObject::connect(&rpc, &mp::DaemonRpc::on_restart, &daemon, &mp::Daemon::restart, Qt::BlockingQueuedConnection);
    QObject::connect(&rpc, &mp::DaemonRpc::on_delete, &daemon, &mp::Daemon::delet, Qt::BlockingQueuedConnection);
    QObject::connect(&rpc, &mp::DaemonRpc::on_umount, &daemon, &mp::Daemon::umount, Qt::BlockingQueuedConnection);
    QObject::connect(&rpc, &mp::DaemonRpc::on_version, &daemon, &mp::Daemon::version, Qt::BlockingQueuedConnection);
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

        // we shouldn't get this far: a successful reboot command does not return
        return grpc::Status{grpc::StatusCode::FAILED_PRECONDITION,
                            fmt::format("Reboot command exited with code {}", ecode), proc.read_std_error()};
    }
    catch (const mp::ExitlessSSHProcessException&)
    {
        // this is the expected path
    }

    return grpc::Status::OK;
}

} // namespace

mp::Daemon::Daemon(std::unique_ptr<const DaemonConfig> the_config)
    : config{std::move(the_config)},
      vm_instance_specs{load_db(config->data_directory, config->cache_directory)},
      daemon_rpc{config->server_address, config->connection_type, *config->cert_provider, *config->client_cert_store},
      metrics_provider{"https://api.staging.jujucharms.com/omnibus/v4/multipass/metrics",
                       get_unique_id(config->data_directory), config->data_directory},
      metrics_opt_in{get_metrics_opt_in(config->data_directory)}
{
    connect_rpc(daemon_rpc, *this);
    std::vector<std::string> invalid_specs;
    bool mac_addr_missing{false};
    for (auto const& entry : vm_instance_specs)
    {
        const auto& name = entry.first;
        const auto& spec = entry.second;

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
        mp::VirtualMachineDescription vm_desc{spec.num_cores, spec.mem_size,  spec.disk_space,
                                              name,           mac_addr,       spec.ssh_username,
                                              vm_image,       cloud_init_iso, *config->ssh_key_provider};

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
    source_images_maintenance_task.start(config->image_refresh_timer);
}

grpc::Status mp::Daemon::launch(grpc::ServerContext* context, const LaunchRequest* request,
                                grpc::ServerWriter<LaunchReply>* server) // clang-format off
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

            return grpc::Status::OK;
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

    auto mem_disk_name_and_errors = validate_create_arguments(request);
    auto mem_size = std::get<0>(mem_disk_name_and_errors); // use structured bindings instead in C++17
    auto disk_space = std::get<1>(mem_disk_name_and_errors); // use structured bindings instead in C++17
    auto instance_name = std::get<2>(mem_disk_name_and_errors); // use structured bindings instead in C++17
    auto option_errors = std::get<3>(mem_disk_name_and_errors); // use structured bindings instead in C++17

    if (!option_errors.error_codes().empty())
    {
        return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Invalid arguments supplied",
                            option_errors.SerializeAsString());
    }

    auto name = name_from(instance_name, *config->name_generator, vm_instances);

    if (vm_instances.find(name) != vm_instances.end() || deleted_instances.find(name) != deleted_instances.end())
    {
        LaunchError create_error;
        create_error.add_error_codes(LaunchError::INSTANCE_EXISTS);

        return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, fmt::format("instance \"{}\" already exists", name),
                            create_error.SerializeAsString());
    }

    auto query = query_from(request, name);

    config->factory->check_hypervisor_support();

    auto progress_monitor = [server](int progress_type, int percentage) {
        LaunchReply create_reply;
        create_reply.mutable_launch_progress()->set_percent_complete(std::to_string(percentage));
        create_reply.mutable_launch_progress()->set_type((LaunchProgress::ProgressTypes)progress_type);
        return server->Write(create_reply);
    };

    auto prepare_action = [this, server, &name](const VMImage& source_image) -> VMImage {
        LaunchReply reply;
        reply.set_create_message("Preparing image for " + name);
        server->Write(reply);

        return config->factory->prepare_source_image(source_image);
    };

    auto fetch_type = config->factory->fetch_type();

    LaunchReply reply;
    reply.set_create_message("Creating " + name);
    server->Write(reply);
    auto vm_image = config->vault->fetch_image(fetch_type, query, prepare_action, progress_monitor);

    reply.set_create_message("Configuring " + name);
    server->Write(reply);
    auto vendor_data_cloud_init_config =
        make_cloud_init_vendor_config(*config->ssh_key_provider, request->time_zone(), config->ssh_username);
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
    auto vm_desc =
        to_machine_desc(request, name, mem_size, disk_space, mac_addr, config->ssh_username, vm_image, meta_data_cloud_init_config,
                        user_data_cloud_init_config, vendor_data_cloud_init_config, *config->ssh_key_provider);

    config->factory->prepare_instance_image(vm_image, vm_desc);

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
    persist_instances();

    reply.set_create_message("Starting " + name);
    server->Write(reply);

    auto& vm = vm_instances[name];
    vm->start();
    vm->wait_until_ssh_up(std::chrono::minutes(5));

    reply.set_create_message("Waiting for initialization to complete");
    server->Write(reply);
    vm->wait_for_cloud_init(std::chrono::minutes(5));

    reply.set_vm_instance_name(name);
    server->Write(reply);

    return grpc::Status::OK;
}
catch (const mp::StartException& e)
{
    auto name = e.name();

    release_resources(name);
    vm_instances.erase(name);
    persist_instances();

    return grpc::Status(grpc::StatusCode::ABORTED, e.what(), "");
}
catch (const std::exception& e)
{
    return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, e.what(), "");
}

grpc::Status mp::Daemon::purge(grpc::ServerContext* context, const PurgeRequest* request,
                               grpc::ServerWriter<PurgeReply>* server) // clang-format off
try // clang-format on
{
    for (const auto& del : deleted_instances)
        release_resources(del.first);

    deleted_instances.clear();
    persist_instances();

    return grpc::Status::OK;
}
catch (const std::exception& e)
{
    return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, e.what(), "");
}

grpc::Status mp::Daemon::find(grpc::ServerContext* context, const FindRequest* request,
                              grpc::ServerWriter<FindReply>* server) // clang-format off
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

            auto images_info =
                it->second->all_info_for({"", request->search_string(), false, remote, Query::Type::Alias});

            if (!images_info.empty())
            {
                vm_images_info = std::move(images_info);
            }
        }
        else
        {
            for (const auto& image_host : config->image_hosts)
            {
                auto images_info =
                    image_host->all_info_for({"", request->search_string(), false, remote, Query::Type::Alias});

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

        auto vm_images_info = it->second->all_images_for(remote);
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
            auto action = [&response, &image_found, default_remote](const std::string& remote,
                                                                    const mp::VMImageInfo& info) {
                if (!mp::platform::is_remote_supported(remote))
                    return;

                if (info.supported)
                {
                    if (image_found.find(info.release_title.toStdString()) == image_found.end())
                    {
                        if (!info.aliases.empty())
                        {
                            auto entry = response.add_images_info();
                            for (const auto& alias : info.aliases)
                            {
                                if (!mp::platform::is_alias_supported(alias.toStdString(), remote))
                                    return;

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
    return grpc::Status::OK;
}
catch (const std::exception& e)
{
    return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, e.what(), "");
}

grpc::Status mp::Daemon::info(grpc::ServerContext* context, const InfoRequest* request,
                              grpc::ServerWriter<InfoReply>* server) // clang-format off
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
            auto status_for = [](mp::VirtualMachine::State state) {
                switch (state)
                {
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
                    return mp::InstanceStatus::UNKNOWN;
                default:
                    return mp::InstanceStatus::STOPPED;
                }
            };
            info->mutable_instance_status()->set_status(status_for(present_state));
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
                mpl::log(mpl::Level::error, category, fmt::format("Error fetching image information: {}", e.what()));
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

    return status;
}
catch (const std::exception& e)
{
    return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, e.what(), "");
}

grpc::Status mp::Daemon::list(grpc::ServerContext* context, const ListRequest* request,
                              grpc::ServerWriter<ListReply>* server) // clang-format off
try // clang-format on
{
    mpl::ClientLogger<ListReply> logger{mpl::level_from(request->verbosity_level()), *config->logger, server};
    ListReply response;

    auto status_for = [](mp::VirtualMachine::State state) {
        switch (state)
        {
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
            return mp::InstanceStatus::UNKNOWN;
        default:
            return mp::InstanceStatus::STOPPED;
        }
    };

    for (const auto& instance : vm_instances)
    {
        const auto& name = instance.first;
        const auto& vm = instance.second;
        auto present_state = vm->current_state();
        auto entry = response.add_instances();
        entry->set_name(name);
        entry->mutable_instance_status()->set_status(status_for(present_state));

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
                mpl::log(mpl::Level::error, category, fmt::format("Error fetching image information: {}", e.what()));
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
    return grpc::Status::OK;
}
catch (const std::exception& e)
{
    return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, e.what(), "");
}

grpc::Status mp::Daemon::mount(grpc::ServerContext* context, const MountRequest* request,
                               grpc::ServerWriter<MountReply>* server) // clang-format off
try // clang-format on
{
    mpl::ClientLogger<MountReply> logger{mpl::level_from(request->verbosity_level()), *config->logger, server};

    QFileInfo source_dir(QString::fromStdString(request->source_path()));
    if (!source_dir.exists())
    {
        return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                            fmt::format("source \"{}\" does not exist", request->source_path()), "");
    }

    if (!source_dir.isDir())
    {
        return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                            fmt::format("source \"{}\" is not a directory", request->source_path()), "");
    }

    if (!source_dir.isReadable())
    {
        return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                            fmt::format("source \"{}\" is not readable", request->source_path()), "");
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

        auto entry = mount_threads.find(name);
        if (entry != mount_threads.end() && entry->second.find(target_path) != entry->second.end())
        {
            fmt::format_to(errors, "\"{}:{}\" is already mounted\n", name, target_path);
            continue;
        }

        auto& vm = it->second;

        if (vm->current_state() == mp::VirtualMachine::State::running)
        {
            try
            {
                start_mount(vm, name, request->source_path(), target_path, gid_map, uid_map);
            }
            catch (const mp::SSHFSMissingError&)
            {
                return grpc_status_for_mount_error(name);
            }
            catch (const std::exception& e)
            {
                fmt::format_to(errors, "error mounting \"{}\": {}", target_path, e.what());
                continue;
            }
        }

        auto& vm_specs = vm_instance_specs[name];
        if (vm_specs.mounts.find(target_path) != vm_specs.mounts.end())
        {
            fmt::format_to(errors, "There is already a mount defined for \"{}:{}\"\n", name, target_path);
            continue;
        }

        VMMount mount{request->source_path(), gid_map, uid_map};
        vm_specs.mounts[target_path] = mount;
    }

    persist_instances();

    return grpc_status_for(errors);
}
catch (const std::exception& e)
{
    return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, e.what(), "");
}

grpc::Status mp::Daemon::recover(grpc::ServerContext* context, const RecoverRequest* request,
                                 grpc::ServerWriter<RecoverReply>* server) // clang-format off
try // clang-format on
{
    mpl::ClientLogger<RecoverReply> logger{mpl::level_from(request->verbosity_level()), *config->logger, server};

    const auto [instances, status] =
        find_requested_instances(request->instance_names().instance_name(), deleted_instances,
                                 std::bind(&Daemon::check_instance_exists, this, std::placeholders::_1));

    if(status.ok())
    {
        for (const auto& name : instances)
        {
            auto it = deleted_instances.find(name);
            if(it != std::end(deleted_instances))
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

    return status;
}
catch (const std::exception& e)
{
    return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, e.what(), "");
}

grpc::Status mp::Daemon::ssh_info(grpc::ServerContext* context, const SSHInfoRequest* request,
                                  grpc::ServerWriter<SSHInfoReply>* server) // clang-format off
try // clang-format on
{
    mpl::ClientLogger<SSHInfoReply> logger{mpl::level_from(request->verbosity_level()), *config->logger, server};
    SSHInfoReply response;

    for (const auto& name : request->instance_name())
    {
        auto it = vm_instances.find(name);
        if (it == vm_instances.end())
        {
            return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, fmt::format("instance \"{}\" does not exist", name),
                                "");
        }

        auto& vm = it->second;
        if (!mp::utils::is_running(vm->current_state()))
        {
            return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION,
                                fmt::format("instance \"{}\" is not running", name), "");
        }

        if (vm->state == VirtualMachine::State::delayed_shutdown)
        {
            if (delayed_shutdown_instances[name]->get_time_remaining() <= std::chrono::minutes(1))
            {
                return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION,
                                    fmt::format("\"{}\" is scheduled to shut down in less than a minute, use "
                                                "'multipass stop --cancel {}' to cancel the shutdown.",
                                                name, name),
                                    "");
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
    return grpc::Status::OK;
}
catch (const std::exception& e)
{
    return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, e.what(), "");
}

grpc::Status mp::Daemon::start(grpc::ServerContext* context, const StartRequest* request,
                               grpc::ServerWriter<StartReply>* server) // clang-format off
try // clang-format on
{
    mpl::ClientLogger<StartReply> logger{mpl::level_from(request->verbosity_level()), *config->logger, server};

    config->factory->check_hypervisor_support();

    std::vector<decltype(vm_instances)::key_type> vms;
    for (const auto& name : request->instance_names().instance_name())
    {
        auto it = vm_instances.find(name);
        if (it == vm_instances.end())
        {
            it = deleted_instances.find(name);
            if (it == deleted_instances.end())
            {
                mp::StartError start_error;
                start_error.set_error_code(mp::StartError::DOES_NOT_EXIST);
                start_error.set_instance_name(name);
                return grpc::Status(grpc::StatusCode::ABORTED, fmt::format("instance \"{}\" does not exist", name),
                                    start_error.SerializeAsString());
            }
            else
            {
                mp::StartError start_error;
                start_error.set_error_code(mp::StartError::INSTANCE_DELETED);
                start_error.set_instance_name(name);
                return grpc::Status(grpc::StatusCode::ABORTED, fmt::format("instance \"{}\" is deleted", name),
                                    start_error.SerializeAsString());
            }
            continue;
        }

        auto present_state = it->second->current_state();
        if (present_state == VirtualMachine::State::running)
        {
            continue;
        }
        else if (present_state == VirtualMachine::State::delayed_shutdown)
        {
            delayed_shutdown_instances.erase(name);
            continue;
        }

        vms.push_back(name);
    }

    if (request->instance_names().instance_name().empty())
    {
        for (auto& pair : vm_instances)
        {
            if (pair.second->current_state() == VirtualMachine::State::running)
                continue;
            vms.push_back(pair.first);
        }
    }

    // Start them all first before we go and do a blocking wait_until_ssh_up call
    for (const auto& name : vms)
    {
        auto it = vm_instances.find(name);
        it->second->start();
    }

    bool update_instance_db{false};
    fmt::memory_buffer errors;
    for (const auto& name : vms)
    {
        auto it = vm_instances.find(name);
        auto& vm = it->second;
        auto& mounts = vm_instance_specs[name].mounts;

        vm->wait_until_ssh_up(std::chrono::minutes(2));

        std::vector<std::string> invalid_mounts;
        for (const auto& mount_entry : mounts)
        {
            auto& target_path = mount_entry.first;
            auto& source_path = mount_entry.second.source_path;
            auto& uid_map = mount_entry.second.uid_map;
            auto& gid_map = mount_entry.second.gid_map;

            try
            {
                start_mount(vm, name, source_path, target_path, gid_map, uid_map);
            }
            catch (const mp::SSHFSMissingError&)
            {
                return grpc_status_for_mount_error(name);
            }
            catch (const std::exception& e)
            {
                fmt::format_to(errors, "Removing \"{}\": {}", target_path, e.what());
                invalid_mounts.push_back(target_path);
            }
        }

        update_instance_db = !invalid_mounts.empty();
        for (const auto& invalid_mount : invalid_mounts)
            mounts.erase(invalid_mount);
    }

    if (update_instance_db)
        persist_instances();

    return grpc_status_for(errors);
}
catch (const std::exception& e)
{
    return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, e.what(), "");
}

grpc::Status mp::Daemon::stop(grpc::ServerContext* context, const StopRequest* request,
                              grpc::ServerWriter<StopReply>* server) // clang-format off
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

    return status;
}
catch (const std::exception& e)
{
    return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, e.what(), "");
}

grpc::Status mp::Daemon::suspend(grpc::ServerContext* context, const SuspendRequest* request,
                                 grpc::ServerWriter<SuspendReply>* server) // clang-format off
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

        for (const auto& name : instances_to_suspend)
        {
            QTimer timer;
            QEventLoop event_loop;

            QObject::connect(this, &Daemon::suspend_finished, &event_loop, &QEventLoop::quit, Qt::QueuedConnection);
            QObject::connect(&timer, &QTimer::timeout, &event_loop, &QEventLoop::quit);

            auto it = vm_instances.find(name);
            it->second->suspend();

            timer.setSingleShot(true);
            timer.start(std::chrono::seconds(30));
            event_loop.exec();
        }
    }

    return status;
}
catch (const std::exception& e)
{
    return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, e.what(), "");
}

grpc::Status mp::Daemon::restart(grpc::ServerContext* context, const RestartRequest* request,
                                 grpc::ServerWriter<RestartReply>* server) // clang-format off
try // clang-format on
{
    mpl::ClientLogger<RestartReply> logger{mpl::level_from(request->verbosity_level()), *config->logger, server};

    auto [instances, status] =
        find_requested_instances(request->instance_names().instance_name(), vm_instances,
                                 std::bind(&Daemon::check_instance_operational, this, std::placeholders::_1));

    if (status.ok())
    {
        status = cmd_vms(instances,
                         std::bind(&Daemon::reboot_vm, this, std::placeholders::_1)); // 1st pass to reboot all targets

        if (status.ok())
        {
            status = cmd_vms(instances, [](auto& vm) {
                // 2nd pass waits for them (only works because SSH was manually killed before rebooting)
                vm.wait_until_ssh_up(up_timeout);

                return grpc::Status::OK;
            });
        }
    }

    return status;
}
catch (const std::exception& e)
{
    return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, e.what(), "");
}

grpc::Status mp::Daemon::delet(grpc::ServerContext* context, const DeleteRequest* request,
                               grpc::ServerWriter<DeleteReply>* server) // clang-format off
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

            stop_mounts_for_instance(name);
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

    return status;
}
catch (const std::exception& e)
{
    return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, e.what(), "");
}

grpc::Status mp::Daemon::umount(grpc::ServerContext* context, const UmountRequest* request,
                                grpc::ServerWriter<UmountReply>* server) // clang-format off
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

        auto stop_sshfs_for = [this, name](const std::string& target_path) {
            auto sshfs_mount_it = mount_threads.find(name);
            if (sshfs_mount_it == mount_threads.end())
            {
                return false;
            }

            auto& sshfs_mount_map = sshfs_mount_it->second;
            auto map_entry = sshfs_mount_map.find(target_path);
            if (map_entry != sshfs_mount_map.end())
            {
                auto& sshfs_mount = map_entry->second;
                sshfs_mount->stop();
                return true;
            }
            return false;
        };

        // Empty target path indicates removing all mounts for the VM instance
        if (target_path.empty())
        {
            stop_mounts_for_instance(name);
            mounts.clear();
        }
        else
        {
            if (vm->current_state() == mp::VirtualMachine::State::running)
            {
                auto found = stop_sshfs_for(target_path);
                if (!found)
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

    return grpc_status_for(errors);
}
catch (const std::exception& e)
{
    return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, e.what(), "");
}

grpc::Status mp::Daemon::version(grpc::ServerContext* context, const VersionRequest* request,
                                 grpc::ServerWriter<VersionReply>* server)
{
    mpl::ClientLogger<VersionReply> logger{mpl::level_from(request->verbosity_level()), *config->logger, server};

    VersionReply reply;
    reply.set_version(multipass::version_string);
    server->Write(reply);
    return grpc::Status::OK;
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
    emit suspend_finished();
}

void mp::Daemon::on_restart(const std::string& name)
{
    auto& vm = vm_instances[name];
    vm->wait_until_ssh_up(std::chrono::minutes(5));

    auto& mounts = vm_instance_specs[name].mounts;
    std::vector<std::string> invalid_mounts;

    for (const auto& mount_entry : mounts)
    {
        auto& target_path = mount_entry.first;
        auto& source_path = mount_entry.second.source_path;
        auto& uid_map = mount_entry.second.uid_map;
        auto& gid_map = mount_entry.second.gid_map;

        try
        {
            start_mount(vm, name, source_path, target_path, gid_map, uid_map);
        }
        catch (const std::exception& e)
        {
            mpl::log(
                mpl::Level::error, name,
                fmt::format("Mount error detected during instance reboot. Removing \"{}\": {}", target_path, e.what()));
            invalid_mounts.push_back(target_path);
        }
    }

    for (const auto& invalid_mount : invalid_mounts)
        mounts.erase(invalid_mount);

    if (!invalid_mounts.empty())
        persist_instances();
}

void mp::Daemon::persist_state_for(const std::string& name)
{
    auto& vm = vm_instances[name];
    vm_instance_specs[name].state = vm->current_state();
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
        json.insert("mem_size", QString::fromStdString(specs.mem_size));
        json.insert("disk_space", QString::fromStdString(specs.disk_space));
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
    QDir data_dir{config->data_directory};
    mp::write_json(instance_records_json, data_dir.filePath(instance_db_name));
}

void mp::Daemon::start_mount(const VirtualMachine::UPtr& vm, const std::string& name, const std::string& source_path,
                             const std::string& target_path, const std::unordered_map<int, int>& gid_map,
                             const std::unordered_map<int, int>& uid_map)
{
    auto& key_provider = *config->ssh_key_provider;

    SSHSession session{vm->ssh_hostname(), vm->ssh_port(), vm->ssh_username(), key_provider};

    mpl::log(mpl::Level::info, category, fmt::format("mounting {} => {} in {}", source_path, target_path, name));

    auto sshfs_mount = std::make_unique<mp::SshfsMount>(std::move(session), source_path, target_path, gid_map, uid_map);
    mount_threads[name][target_path] = std::move(sshfs_mount);

    QObject::connect(mount_threads[name][target_path].get(), &SshfsMount::finished, this,
                     [this, name, target_path]() {
                         mount_threads[name].erase(target_path);
                         mpl::log(mpl::Level::debug, category,
                                  fmt::format("Mount stopped: '{}' in instance \"{}\"", target_path, name));
                     },
                     Qt::QueuedConnection);
}

void mp::Daemon::stop_mounts_for_instance(const std::string& instance)
{
    auto mounts_it = mount_threads.find(instance);
    if (mounts_it == mount_threads.end() || mounts_it->second.empty())
    {
        mpl::log(mpl::Level::debug, category, fmt::format("No mounts to stop for instance \"{}\"", instance));
    }
    else
    {
        for (auto& sshfs_mount : mounts_it->second)
        {
            mpl::log(mpl::Level::debug, category,
                     fmt::format("Stopping mount '{}' in instance \"{}\"", sshfs_mount.first, instance));
            sshfs_mount.second->stop();
        }
    }
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
    if(vm_instances.find(instance_name) == std::cend(vm_instances) &&
        deleted_instances.find(instance_name) == std::cend(deleted_instances))
        return fmt::format("instance \"{}\" does not exist\n", instance_name);

    return {};
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

        auto& shutdown_timer = delayed_shutdown_instances[name] =
            std::make_unique<DelayedShutdownTimer>(&vm, std::move(session));

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
