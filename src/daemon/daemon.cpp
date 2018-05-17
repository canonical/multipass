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
 * Authored by: Alberto Aguirre <alberto.aguirre@canonical.com>
 *
 */

#include "daemon.h"
#include "base_cloud_init_config.h"
#include "json_writer.h"

#include <multipass/cloud_init_iso.h>
#include <multipass/exceptions/sshfs_missing_error.h>
#include <multipass/exceptions/start_exception.h>
#include <multipass/logging/log.h>
#include <multipass/name_generator.h>
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
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

#include <stdexcept>

namespace mp = multipass;
namespace mpl = multipass::logging;

namespace
{
constexpr auto category = "daemon";
constexpr auto instance_db_name = "multipassd-vm-instances.json";

mp::Query query_from(const mp::LaunchRequest* request, const std::string& name)
{
    if (!request->remote_name().empty() && request->image().empty())
        throw std::runtime_error("Must specify an image when specifying a remote");

    std::string image = !request->custom_image_path().empty()
                            ? request->custom_image_path()
                            : (request->image().empty() ? "default" : request->image());
    // TODO: persistence should be specified by the rpc as well

    mp::Query::Type query_type{mp::Query::Type::SimpleStreams};

    if (!request->custom_image_path().empty())
    {
        QString custom_image_path = QString::fromStdString(request->custom_image_path());

        if (custom_image_path.startsWith("file"))
            query_type = mp::Query::Type::LocalFile;
        else if (custom_image_path.startsWith("http"))
            query_type = mp::Query::Type::HttpDownload;
    }

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
                                              const std::string& mac_addr, const std::string& ssh_username,
                                              const mp::VMImage& image, YAML::Node& meta_data_config,
                                              YAML::Node& user_data_config, YAML::Node& vendor_data_config,
                                              const mp::SSHKeyProvider& key_provider)
{
    const auto num_cores = request->num_cores() < 1 ? 1 : request->num_cores();
    const auto mem_size = request->mem_size().empty() ? "1G" : request->mem_size();
    const auto disk_size = request->disk_space().empty() ? "5G" : request->disk_space();
    const auto instance_dir = mp::utils::base_dir(image.image_path);
    const auto cloud_init_iso =
        make_cloud_init_image(name, instance_dir, meta_data_config, user_data_config, vendor_data_config);
    return {num_cores, mem_size, disk_size, name, mac_addr, ssh_username, image, cloud_init_iso, key_provider};
}

template <typename T>
auto name_from(const mp::LaunchRequest* request, mp::NameGenerator& name_gen, const T& currently_used_names)
{
    auto requested_name = request->instance_name();
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
                                      mounts};
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
    mp::LaunchError launch_error;

    if (!request->disk_space().empty() && !mp::utils::valid_memory_value(QString::fromStdString(request->disk_space())))
    {
        launch_error.add_error_codes(mp::LaunchError::INVALID_DISK_SIZE);
    }

    if (!request->mem_size().empty() && !mp::utils::valid_memory_value(QString::fromStdString(request->mem_size())))
    {
        launch_error.add_error_codes(mp::LaunchError::INVALID_MEM_SIZE);
    }

    if (!request->instance_name().empty() &&
        !mp::utils::valid_hostname(QString::fromStdString(request->instance_name())))
    {
        launch_error.add_error_codes(mp::LaunchError::INVALID_HOSTNAME);
    }

    return launch_error;
}

auto grpc_status_for_mount_error(const std::string& instance_name)
{
    mp::MountError mount_error;
    mount_error.set_error_code(mp::MountError::SSHFS_MISSING);
    mount_error.set_instance_name(instance_name);

    return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, "Mount failed", mount_error.SerializeAsString());
}

auto grpc_status_for(fmt::MemoryWriter& errors)
{
    // Remove trailing newline due to grpc adding one of it's own
    auto error_string = errors.str();
    if (error_string.back() == '\n')
        error_string.pop_back();

    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                        fmt::format("The following errors occured:\n{}", error_string), "");
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
    QObject::connect(&rpc, &mp::DaemonRpc::on_delete, &daemon, &mp::Daemon::delet, Qt::BlockingQueuedConnection);
    QObject::connect(&rpc, &mp::DaemonRpc::on_umount, &daemon, &mp::Daemon::umount, Qt::BlockingQueuedConnection);
    QObject::connect(&rpc, &mp::DaemonRpc::on_version, &daemon, &mp::Daemon::version, Qt::BlockingQueuedConnection);
}
}

mp::Daemon::Daemon(std::unique_ptr<const DaemonConfig> the_config)
    : config{std::move(the_config)},
      vm_instance_specs{load_db(config->data_directory, config->cache_directory)},
      daemon_rpc{config->server_address}
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
            vm_instances[name] = config->factory->create_virtual_machine(vm_desc, *this);
        }
        catch (const std::exception& e)
        {
            mpl::log(mpl::Level::error, category, fmt::format("Removing instance {}: {}", name, e.what()));
            invalid_specs.push_back(name);
            config->vault->remove(name);
        }
    }

    for (const auto& bad_spec : invalid_specs)
    {
        vm_instance_specs.erase(bad_spec);
    }

    if (!invalid_specs.empty() || mac_addr_missing)
        persist_instances();

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
    const std::chrono::milliseconds ms = std::chrono::hours(6);
    source_images_maintenance_task.start(ms.count());
}

grpc::Status mp::Daemon::launch(grpc::ServerContext* context, const LaunchRequest* request,
                                grpc::ServerWriter<LaunchReply>* server) // clang-format off
try // clang-format on
{
    auto name = name_from(request, *config->name_generator, vm_instances);

    if (vm_instances.find(name) != vm_instances.end() || deleted_instances.find(name) != deleted_instances.end())
    {
        LaunchError create_error;
        create_error.add_error_codes(LaunchError::INSTANCE_EXISTS);

        return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, fmt::format("instance \"{}\" already exists", name),
                            create_error.SerializeAsString());
    }

    config->factory->check_hypervisor_support();

    auto option_errors = validate_create_arguments(request);

    if (!option_errors.error_codes().empty())
    {
        return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Invalid arguments supplied",
                            option_errors.SerializeAsString());
    }

    auto query = query_from(request, name);
    auto download_monitor = [server](int download_type, int percentage) {
        LaunchReply create_reply;
        create_reply.mutable_download_progress()->set_percent_complete(std::to_string(percentage));
        create_reply.mutable_download_progress()->set_type((DownloadProgress::DownloadTypes)download_type);
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
    auto vm_image = config->vault->fetch_image(fetch_type, query, prepare_action, download_monitor);

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
        to_machine_desc(request, name, mac_addr, config->ssh_username, vm_image, meta_data_cloud_init_config,
                        user_data_cloud_init_config, vendor_data_cloud_init_config, *config->ssh_key_provider);

    config->factory->prepare_instance_image(vm_image, vm_desc);

    vm_instances[name] = config->factory->create_virtual_machine(vm_desc, *this);
    vm_instance_specs[name] = {vm_desc.num_cores, vm_desc.mem_size,     vm_desc.disk_space,
                               vm_desc.mac_addr,  config->ssh_username, {}};
    persist_instances();

    reply.set_create_message("Starting " + name);
    server->Write(reply);

    auto& vm = vm_instances[name];
    vm->start();
    vm->wait_until_ssh_up(std::chrono::minutes(5));

    reply.set_create_message("Waiting for cloud-init to complete");
    server->Write(reply);
    vm->wait_for_cloud_init(std::chrono::minutes(5));

    reply.set_vm_instance_name(name);
    server->Write(reply);

    return grpc::Status::OK;
}
catch (const mp::StartException& e)
{
    auto name = e.name();

    config->factory->remove_resources_for(name);
    config->vault->remove(name);
    vm_instance_specs.erase(name);
    vm_instances.erase(name);
    persist_instances();

    return grpc::Status(grpc::StatusCode::ABORTED, e.what(), "");
}
catch (const std::exception& e)
{
    return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, e.what(), "");
}

grpc::Status mp::Daemon::purge(grpc::ServerContext* context, const PurgeRequest* request,
                               PurgeReply* response) // clang-format off
try // clang-format on
{
    std::vector<decltype(deleted_instances)::key_type> keys_to_delete;
    for (auto& del : deleted_instances)
    {
        const auto& name = del.first;
        config->factory->remove_resources_for(name);
        config->vault->remove(name);
        keys_to_delete.push_back(name);
    }

    for (auto const& key : keys_to_delete)
    {
        deleted_instances.erase(key);
        vm_instance_specs.erase(key);
    }

    persist_instances();
    return grpc::Status::OK;
}
catch (const std::exception& e)
{
    return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, e.what(), "");
}

grpc::Status mp::Daemon::find(grpc::ServerContext* context, const FindRequest* request,
                              FindReply* response) // clang-format off
try // clang-format on
{
    if (!request->search_string().empty())
    {
        auto vm_images_info = config->image_host->all_info_for(
            {"", request->search_string(), false, request->remote_name(), Query::Type::SimpleStreams});

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

            auto entry = response->add_images_info();
            entry->set_release(info.release_title.toStdString());
            entry->set_version(info.version.toStdString());
            auto alias_entry = entry->add_aliases_info();
            alias_entry->set_remote_name(request->remote_name());
            alias_entry->set_alias(name);
        }
    }
    else
    {
        std::unordered_map<std::string, bool> image_found;
        const auto remote_name{request->remote_name()};
        const auto default_remote{config->image_host->get_default_remote()};
        auto action = [&response, &image_found, remote_name, default_remote](const std::string& remote,
                                                                             const mp::VMImageInfo& info) {
            if (!remote_name.empty() && remote_name != remote)
                return;

            if (info.supported)
            {
                if (image_found.find(info.release_title.toStdString()) == image_found.end())
                {
                    if (!info.aliases.empty())
                    {
                        image_found[info.release_title.toStdString()] = true;
                        auto entry = response->add_images_info();
                        entry->set_release(info.release_title.toStdString());
                        entry->set_version(info.version.toStdString());

                        for (const auto& alias : info.aliases)
                        {
                            auto alias_entry = entry->add_aliases_info();
                            alias_entry->set_remote_name((remote == default_remote) ? "" : remote);
                            alias_entry->set_alias(alias.toStdString());
                        }
                    }
                }
            }
        };
        config->image_host->for_each_entry_do(action);
    }
    return grpc::Status::OK;
}
catch (const std::exception& e)
{
    return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, e.what(), "");
}

grpc::Status mp::Daemon::info(grpc::ServerContext* context, const InfoRequest* request,
                              InfoReply* response) // clang-format off
try // clang-format on
{
    fmt::MemoryWriter errors;
    std::vector<decltype(vm_instances)::key_type> instances_for_info;

    if (request->instance_name().empty())
    {
        for (auto& pair : vm_instances)
            instances_for_info.push_back(pair.first);
    }
    else
    {
        for (const auto& name : request->instance_name())
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
                errors.write("instance \"{}\" does not exist\n", name);
                continue;
            }
            deleted = true;
        }

        auto info = response->add_info();
        auto& vm = it->second;
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
                case mp::VirtualMachine::State::unknown:
                    return mp::InstanceStatus::UNKNOWN;
                default:
                    return mp::InstanceStatus::STOPPED;
                }
            };
            info->mutable_instance_status()->set_status(status_for(vm->current_state()));
        }

        auto vm_image = fetch_image_for(name, config->factory->fetch_type(), *config->vault);
        auto original_release = vm_image.original_release;

        if (!vm_image.id.empty() && original_release.empty())
        {
            try
            {
                auto vm_image_info = config->image_host->info_for_full_hash(vm_image.id);
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
        }

        if (vm->current_state() == mp::VirtualMachine::State::running)
        {
            mp::SSHSession session{vm->ssh_hostname(), vm->ssh_port(), vm_specs.ssh_username,
                                   *config->ssh_key_provider};

            auto run_in_vm = [&session](const std::string& cmd) {
                auto proc = session.exec(cmd);
                if (auto exit_code = proc.exit_code() != 0)
                    throw std::runtime_error(fmt::format("failed to run '{}', exit code:{}", cmd, exit_code));

                auto output = proc.read_std_output();
                if (output.empty())
                    throw std::runtime_error(fmt::format("no output after running '{}'", cmd));

                return mp::utils::trim_end(output);
            };

            info->set_load(run_in_vm("cat /proc/loadavg | cut -d ' ' -f1-3"));
            info->set_memory_usage(run_in_vm("free -b | sed '1d;3d' | awk '{printf $3}'"));
            info->set_memory_total(run_in_vm("free -b | sed '1d;3d' | awk '{printf $2}'"));
            info->set_disk_usage(run_in_vm("df --output=used `awk '$2 == \"/\" { print $1 }' /proc/mounts` -B1 | sed 1d"));
            info->set_disk_total(run_in_vm("df --output=size `awk '$2 == \"/\" { print $1 }' /proc/mounts` -B1 | sed 1d"));
            info->set_current_release(run_in_vm("lsb_release -ds"));
            info->set_ipv4(vm->ipv4());
        }
    }

    if (errors.size() > 0)
        return grpc_status_for(errors);
    return grpc::Status::OK;
}
catch (const std::exception& e)
{
    return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, e.what(), "");
}

grpc::Status mp::Daemon::list(grpc::ServerContext* context, const ListRequest* request,
                              ListReply* response) // clang-format off
try // clang-format on
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
        auto entry = response->add_instances();
        entry->set_name(name);
        entry->mutable_instance_status()->set_status(status_for(vm->current_state()));

        // FIXME: Set the release to the cached current version when supported
        auto vm_image = fetch_image_for(name, config->factory->fetch_type(), *config->vault);
        auto current_release = vm_image.original_release;

        if (!vm_image.id.empty() && current_release.empty())
        {
            try
            {
                auto vm_image_info = config->image_host->info_for_full_hash(vm_image.id);
                current_release = vm_image_info.release_title.toStdString();
            }
            catch (const std::exception& e)
            {
                mpl::log(mpl::Level::error, category, fmt::format("Error fetching image information: {}", e.what()));
            }
        }

        entry->set_current_release(current_release);

        if (vm->current_state() == mp::VirtualMachine::State::running)
            entry->set_ipv4(vm->ipv4());
    }

    for (const auto& instance : deleted_instances)
    {
        const auto& name = instance.first;
        auto entry = response->add_instances();
        entry->set_name(name);
        entry->mutable_instance_status()->set_status(mp::InstanceStatus::DELETED);
    }

    return grpc::Status::OK;
}
catch (const std::exception& e)
{
    return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, e.what(), "");
}

grpc::Status mp::Daemon::mount(grpc::ServerContext* context, const MountRequest* request,
                               MountReply* response) // clang-format off
try // clang-format on
{
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

    std::unordered_map<int, int> gid_map;
    std::unordered_map<int, int> uid_map;

    for (const auto& map : request->gid_maps())
    {
        gid_map[map.host_gid()] = map.instance_gid();
    }

    for (const auto& map : request->uid_maps())
    {
        uid_map[map.host_uid()] = map.instance_uid();
    }

    fmt::MemoryWriter errors;
    for (const auto& path_entry : request->target_paths())
    {
        const auto name = path_entry.instance_name();
        auto it = vm_instances.find(name);
        if (it == vm_instances.end())
        {
            errors.write("instance \"{}\" does not exist\n", name);
            continue;
        }

        auto target_path = path_entry.target_path();
        if (mp::utils::invalid_target_path(QString::fromStdString(target_path)))
        {
            errors.write("Unable to mount to \"{}\"\n", target_path);
            continue;
        }

        auto entry = mount_threads.find(name);
        if (entry != mount_threads.end() && entry->second.find(target_path) != entry->second.end())
        {
            errors.write("\"{}:{}\" is already mounted\n", name, target_path);
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
                errors.write("error mounting \"{}\": {}", target_path, e.what());
                continue;
            }
        }

        auto& vm_specs = vm_instance_specs[name];
        if (vm_specs.mounts.find(target_path) != vm_specs.mounts.end())
        {
            errors.write("There is already a mount defined for \"{}:{}\"\n", name, target_path);
            continue;
        }

        VMMount mount{request->source_path(), gid_map, uid_map};
        vm_specs.mounts[target_path] = mount;
    }

    persist_instances();

    if (errors.size() > 0)
        return grpc_status_for(errors);

    return grpc::Status::OK;
}
catch (const std::exception& e)
{
    return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, e.what(), "");
}

grpc::Status mp::Daemon::recover(grpc::ServerContext* context, const RecoverRequest* request,
                                 RecoverReply* response) // clang-format off
try // clang-format on
{
    fmt::MemoryWriter errors;
    std::vector<decltype(deleted_instances)::key_type> instances_to_recover;
    for (const auto& name : request->instance_name())
    {
        auto it = deleted_instances.find(name);
        if (it == deleted_instances.end())
        {
            it = vm_instances.find(name);
            if (it == vm_instances.end())
                errors.write("instance \"{}\" does not exist\n", name);
            else
                errors.write("instance \"{}\" has not been deleted\n", name);
            continue;
        }
        instances_to_recover.push_back(name);
    }

    if (errors.size() > 0)
        return grpc_status_for(errors);

    if (instances_to_recover.empty())
    {
        for (auto& pair : deleted_instances)
            instances_to_recover.push_back(pair.first);
    }

    for (const auto& name : instances_to_recover)
    {
        auto it = deleted_instances.find(name);
        it->second->shutdown();
        vm_instances[name] = std::move(it->second);
        deleted_instances.erase(name);
    }

    return grpc::Status::OK;
}
catch (const std::exception& e)
{
    return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, e.what(), "");
}

grpc::Status mp::Daemon::ssh_info(grpc::ServerContext* context, const SSHInfoRequest* request,
                                  SSHInfoReply* response) // clang-format off
try // clang-format on
{
    for (const auto& name : request->instance_name())
    {
        auto it = vm_instances.find(name);
        if (it == vm_instances.end())
        {
            return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, fmt::format("instance \"{}\" does not exist", name),
                                "");
        }

        if (it->second->current_state() != mp::VirtualMachine::State::running)
        {
            return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION,
                                fmt::format("instance \"{}\" is not running", name), "");
        }

        mp::SSHInfo ssh_info;
        ssh_info.set_host(it->second->ssh_hostname());
        ssh_info.set_port(it->second->ssh_port());
        ssh_info.set_priv_key_base64(config->ssh_key_provider->private_key_as_base64());
        ssh_info.set_username(it->second->ssh_username());
        (*response->mutable_ssh_info())[name] = ssh_info;
    }

    return grpc::Status::OK;
}
catch (const std::exception& e)
{
    return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, e.what(), "");
}

grpc::Status mp::Daemon::start(grpc::ServerContext* context, const StartRequest* request,
                               StartReply* response) // clang-format off
try // clang-format on
{
    config->factory->check_hypervisor_support();

    std::vector<decltype(vm_instances)::key_type> vms;
    for (const auto& name : request->instance_name())
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

        if (it->second->current_state() == VirtualMachine::State::running)
            continue;

        vms.push_back(name);
    }

    if (request->instance_name().empty())
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
    fmt::MemoryWriter errors;
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
                errors.write("Removing \"{}\": {}", target_path, e.what());
                invalid_mounts.push_back(target_path);
            }
        }

        update_instance_db = !invalid_mounts.empty();
        for (const auto& invalid_mount : invalid_mounts)
            mounts.erase(invalid_mount);
    }

    if (update_instance_db)
        persist_instances();

    if (errors.size() > 0)
        return grpc_status_for(errors);

    return grpc::Status::OK;
}
catch (const std::exception& e)
{
    return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, e.what(), "");
}

grpc::Status mp::Daemon::stop(grpc::ServerContext* context, const StopRequest* request,
                              StopReply* response) // clang-format off
try // clang-format on
{
    fmt::MemoryWriter errors;
    std::vector<decltype(vm_instances)::key_type> instances_to_stop;
    for (const auto& name : request->instance_name())
    {
        auto it = vm_instances.find(name);
        if (it == vm_instances.end())
        {
            it = deleted_instances.find(name);
            if (it == deleted_instances.end())
                errors.write("instance \"{}\" does not exist\n", name);
            else
                errors.write("instance \"{}\" is deleted\n", name);
            continue;
        }
        instances_to_stop.push_back(name);
    }

    if (errors.size() > 0)
        return grpc_status_for(errors);

    if (instances_to_stop.empty())
    {
        for (auto& pair : vm_instances)
            instances_to_stop.push_back(pair.first);
    }

    for (const auto& name : instances_to_stop)
    {
        auto it = vm_instances.find(name);
        it->second->shutdown();
    }

    return grpc::Status::OK;
}
catch (const std::exception& e)
{
    return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, e.what(), "");
}

grpc::Status mp::Daemon::delet(grpc::ServerContext* context, const DeleteRequest* request,
                               DeleteReply* response) // clang-format off
try // clang-format on
{
    fmt::MemoryWriter errors;
    std::vector<decltype(vm_instances)::key_type> instances_to_delete;
    for (const auto& name : request->instance_name())
    {
        auto it = vm_instances.find(name);
        if (it == vm_instances.end())
        {
            errors.write("instance \"{}\" does not exist\n", name);
            continue;
        }
        instances_to_delete.push_back(name);
    }

    if (errors.size() > 0)
        return grpc_status_for(errors);

    if (instances_to_delete.empty())
    {
        for (auto& pair : vm_instances)
            instances_to_delete.push_back(pair.first);
    }

    const bool purge = request->purge();
    for (const auto& name : instances_to_delete)
    {
        auto it = vm_instances.find(name);
        it->second->shutdown();
        if (purge)
        {
            config->factory->remove_resources_for(name);
            config->vault->remove(name);
            vm_instance_specs.erase(name);
        }
        else
        {
            deleted_instances[name] = std::move(it->second);
        }
        vm_instances.erase(name);
    }

    if (purge)
        persist_instances();

    return grpc::Status::OK;
}
catch (const std::exception& e)
{
    return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, e.what(), "");
}

grpc::Status mp::Daemon::umount(grpc::ServerContext* context, const UmountRequest* request,
                                UmountReply* response) // clang-format off
try // clang-format on
{
    fmt::MemoryWriter errors;
    for (const auto& path_entry : request->target_paths())
    {
        const auto name = path_entry.instance_name();
        auto it = vm_instances.find(name);
        if (it == vm_instances.end())
        {
            errors.write("instance \"{}\" does not exist\n", name);
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
            if (vm->current_state() == mp::VirtualMachine::State::running)
            {
                for (const auto& mount : mounts)
                {
                    auto& target_path = mount.first;
                    stop_sshfs_for(target_path);
                }
            }

            mounts.clear();
        }
        else
        {
            if (vm->current_state() == mp::VirtualMachine::State::running)
            {
                auto found = stop_sshfs_for(target_path);
                if (!found)
                {
                    errors.write("\"{}\" is not mounted\n", target_path);
                }
            }

            auto erased = mounts.erase(target_path);
            if (!erased)
            {
                errors.write("\"{}\" not found in database\n", target_path);
            }
        }
    }

    persist_instances();

    if (errors.size() > 0)
        return grpc_status_for(errors);
    return grpc::Status::OK;
}
catch (const std::exception& e)
{
    return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, e.what(), "");
}

grpc::Status mp::Daemon::version(grpc::ServerContext* context, const VersionRequest* request, VersionReply* response)
{
    response->set_version(multipass::version_string);
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

void mp::Daemon::persist_instances()
{
    auto vm_spec_to_json = [](const mp::VMSpecs& specs) -> QJsonObject {
        QJsonObject json;
        json.insert("num_cores", specs.num_cores);
        json.insert("mem_size", QString::fromStdString(specs.mem_size));
        json.insert("disk_space", QString::fromStdString(specs.disk_space));
        json.insert("mac_addr", QString::fromStdString(specs.mac_addr));
        json.insert("ssh_username", QString::fromStdString(specs.ssh_username));

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

    auto sshfs_mount = std::make_unique<mp::SshfsMount>(std::move(session), source_path, target_path, gid_map, uid_map);

    QObject::connect(sshfs_mount.get(), &SshfsMount::finished, this,
                     [this, name, target_path]() { mount_threads[name].erase(target_path); });

    mount_threads[name][target_path] = std::move(sshfs_mount);
}
