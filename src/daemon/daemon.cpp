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

#include <yaml-cpp/yaml.h>

#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

#include <sstream>
#include <stdexcept>

namespace mp = multipass;

namespace
{
constexpr auto instance_db_name = "multipassd-vm-instances.json";

mp::Query query_from(const mp::CreateRequest* request, const std::string& name)
{
    if (!request->remote_name().empty() && request->image().empty())
        throw std::runtime_error("Must specify an image when specifying a remote");

    std::string image = request->image().empty() ? "default" : request->image();
    // TODO: persistence should be specified by the rpc as well
    return {name, image, false, request->remote_name()};
}

auto make_cloud_init_vendor_config(const mp::SSHKeyProvider& key_provider, const std::string& time_zone)
{
    auto config = YAML::Load(mp::base_cloud_init_config);
    std::stringstream ssh_key_line;
    ssh_key_line << "ssh-rsa"
                 << " " << key_provider.public_key_as_base64() << " "
                 << "multipass@localhost";
    config["ssh_authorized_keys"].push_back(ssh_key_line.str());

    config["timezone"] = time_zone;

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
    using namespace std::string_literals;
    YAML::Emitter emitter;
    emitter.SetIndent(4);
    emitter << node;
    if (!emitter.good())
    {
        throw std::runtime_error{"Failed to emit "s + node_name + " cloud-init config: "s + emitter.GetLastError()};
    }

    std::stringstream stream;
    stream << "#cloud-config\n" << emitter.c_str() << "\n";
    return stream.str();
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

void prepare_user_data(YAML::Node& user_data_config)
{
    auto users = user_data_config["users"];
    if (users.IsSequence())
        users.push_back("default");

    auto packages = user_data_config["packages"];
    if (packages.IsSequence())
        packages.push_back("sshfs");
}

mp::VirtualMachineDescription to_machine_desc(const mp::CreateRequest* request, const std::string& name,
                                              const mp::VMImage& image, YAML::Node& meta_data_config,
                                              YAML::Node& user_data_config, YAML::Node& vendor_data_config)
{
    const auto num_cores = request->num_cores() < 1 ? 1 : request->num_cores();
    const auto mem_size = request->mem_size().empty() ? "1G" : request->mem_size();
    const auto disk_size = request->disk_space();
    const auto instance_dir = mp::utils::base_dir(image.image_path);
    const auto cloud_init_iso =
        make_cloud_init_image(name, instance_dir, meta_data_config, user_data_config, vendor_data_config);
    return {num_cores, mem_size, disk_size, name, image, cloud_init_iso};
}

template <typename T>
auto name_from(const mp::CreateRequest* request, mp::NameGenerator& name_gen, const T& currently_used_names)
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

        reconstructed_records[key] = {num_cores, mem_size.toStdString(), disk_space.toStdString(), mounts};
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

auto validate_create_arguments(const mp::CreateRequest* request)
{
    mp::CreateError create_error;

    if (!request->disk_space().empty() && !mp::utils::valid_memory_value(QString::fromStdString(request->disk_space())))
    {
        create_error.add_error_codes(mp::CreateError::INVALID_DISK_SIZE);
    }

    if (!request->mem_size().empty() && !mp::utils::valid_memory_value(QString::fromStdString(request->mem_size())))
    {
        create_error.add_error_codes(mp::CreateError::INVALID_MEM_SIZE);
    }

    if (!request->instance_name().empty() &&
        !mp::utils::valid_hostname(QString::fromStdString(request->instance_name())))
    {
        create_error.add_error_codes(mp::CreateError::INVALID_HOSTNAME);
    }

    return create_error;
}

auto handle_mount_error(const std::string& instance_name)
{
    mp::MountError mount_error;
    mount_error.set_error_code(mp::MountError::SSHFS_MISSING);
    mount_error.set_instance_name(instance_name);

    return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, "Mount failed", mount_error.SerializeAsString());
}

void wait_until_cloud_init_finished(const std::string& host, int port, const mp::SSHKeyProvider& key_provider,
                                    std::chrono::milliseconds timeout)
{
    using namespace std::literals::chrono_literals;
    auto deadline = std::chrono::steady_clock::now() + timeout;
    bool cloud_init_finished{false};

    while (std::chrono::steady_clock::now() < deadline)
    {
        mp::SSHSession session{host, port, key_provider};
        auto ssh_process =
            session.exec({"[ -e /var/lib/cloud/instance/boot-finished ]"});
        if (ssh_process.exit_code() == 0)
        {
            cloud_init_finished = true;
            break;
        }

        std::this_thread::sleep_for(1s);
    }

    if (!cloud_init_finished)
        throw std::runtime_error("Timed out waiting for cloud-init to complete");
}
}

mp::DaemonRunner::DaemonRunner(std::unique_ptr<const DaemonConfig>& config, Daemon* daemon)
    : daemon_rpc{config->server_address, config->cout, config->cerr}, daemon_thread{[this, daemon] {
          QObject::connect(&daemon_rpc, &DaemonRpc::on_create, daemon, &Daemon::create, Qt::BlockingQueuedConnection);
          QObject::connect(&daemon_rpc, &DaemonRpc::on_empty_trash, daemon, &Daemon::empty_trash,
                           Qt::BlockingQueuedConnection);
          QObject::connect(&daemon_rpc, &DaemonRpc::on_find, daemon, &Daemon::find, Qt::BlockingQueuedConnection);
          QObject::connect(&daemon_rpc, &DaemonRpc::on_info, daemon, &Daemon::info, Qt::BlockingQueuedConnection);
          QObject::connect(&daemon_rpc, &DaemonRpc::on_list, daemon, &Daemon::list, Qt::BlockingQueuedConnection);
          QObject::connect(&daemon_rpc, &DaemonRpc::on_mount, daemon, &Daemon::mount, Qt::BlockingQueuedConnection);
          QObject::connect(&daemon_rpc, &DaemonRpc::on_recover, daemon, &Daemon::recover, Qt::BlockingQueuedConnection);
          QObject::connect(&daemon_rpc, &DaemonRpc::on_ssh_info, daemon, &Daemon::ssh_info,
                           Qt::BlockingQueuedConnection);
          QObject::connect(&daemon_rpc, &DaemonRpc::on_start, daemon, &Daemon::start, Qt::BlockingQueuedConnection);
          QObject::connect(&daemon_rpc, &DaemonRpc::on_stop, daemon, &Daemon::stop, Qt::BlockingQueuedConnection);
          QObject::connect(&daemon_rpc, &DaemonRpc::on_trash, daemon, &Daemon::trash, Qt::BlockingQueuedConnection);
          QObject::connect(&daemon_rpc, &DaemonRpc::on_umount, daemon, &Daemon::umount, Qt::BlockingQueuedConnection);
          QObject::connect(&daemon_rpc, &DaemonRpc::on_version, daemon, &Daemon::version, Qt::BlockingQueuedConnection);
          daemon_rpc.run();
      }}
{
}

mp::DaemonRunner::~DaemonRunner()
{
    daemon_rpc.shutdown();
}

mp::Daemon::Daemon(std::unique_ptr<const DaemonConfig> the_config)
    : config{std::move(the_config)},
      vm_instance_specs{load_db(config->data_directory, config->cache_directory)},
      runner(config, this)
{
    std::vector<std::string> invalid_specs;
    for (auto const& entry : vm_instance_specs)
    {
        const auto& name = entry.first;
        const auto& spec = entry.second;

        if (!config->vault->has_record_for(name))
        {
            invalid_specs.push_back(name);
            continue;
        }

        auto vm_image = fetch_image_for(name, config->factory->fetch_type(), *config->vault);
        const auto instance_dir = mp::utils::base_dir(vm_image.image_path);
        const auto cloud_init_iso = instance_dir.filePath("cloud-init-config.iso");
        mp::VirtualMachineDescription vm_desc{spec.num_cores, spec.mem_size, spec.disk_space,
                                              name,           vm_image,      cloud_init_iso};

        try
        {
            vm_instances[name] = config->factory->create_virtual_machine(vm_desc, *this);
        }
        catch (const std::exception& e)
        {
            config->cerr << "Removing instance " << name << ": " << e.what() << "\n";
            invalid_specs.push_back(name);
            config->vault->remove(name);
        }
    }

    for (const auto& bad_spec : invalid_specs)
    {
        vm_instance_specs.erase(bad_spec);
    }

    if (!invalid_specs.empty())
        persist_instances();

    config->vault->prune_expired_images();

    // Fire timer every six hours to perform maintenance on source images such as
    // pruning expired images and updating to newly released images.
    connect(&source_images_maintenance_task, &QTimer::timeout, [this]() {
        config->vault->prune_expired_images();

        auto prepare_action = [this](const VMImage& source_image) -> VMImage {
            return config->factory->prepare_source_image(source_image);
        };

        auto download_monitor = [this](int download_type, int percentage) {
            static bool done_once = false;
            if (percentage % 10 == 0)
            {
                if (!done_once)
                {
                    config->cout << std::to_string(percentage) << "%";
                    done_once = true;
                    if (percentage == 100)
                        config->cout << "\n";
                    else
                        config->cout << "..." << std::flush;
                }
            }
            else
            {
                done_once = false;
            }
            return true;
        };
        try
        {
            config->vault->update_images(config->factory->fetch_type(), prepare_action, download_monitor);
        }
        catch (const std::exception& e)
        {
            config->cerr << "Error updating images: " << e.what() << "\n";
        }
    });
    const std::chrono::milliseconds ms = std::chrono::hours(6);
    source_images_maintenance_task.start(ms.count());
}

grpc::Status mp::Daemon::create(grpc::ServerContext* context, const CreateRequest* request,
                                grpc::ServerWriter<CreateReply>* server) // clang-format off
try // clang-format on
{
    auto name = name_from(request, *config->name_generator, vm_instances);

    if (vm_instances.find(name) != vm_instances.end() || vm_instance_trash.find(name) != vm_instance_trash.end())
    {
        CreateError create_error;
        create_error.add_error_codes(CreateError::INSTANCE_EXISTS);

        return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "instance \"" + name + "\" already exists",
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
        CreateReply create_reply;
        create_reply.mutable_download_progress()->set_percent_complete(std::to_string(percentage));
        create_reply.mutable_download_progress()->set_type((DownloadProgress::DownloadTypes)download_type);
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
    auto vm_image = config->vault->fetch_image(fetch_type, query, prepare_action, download_monitor);

    reply.set_create_message("Configuring " + name);
    server->Write(reply);
    auto vendor_data_cloud_init_config = make_cloud_init_vendor_config(*config->ssh_key_provider, request->time_zone());
    auto meta_data_cloud_init_config = make_cloud_init_meta_config(name);
    auto user_data_cloud_init_config = YAML::Load(request->cloud_init_user_data());
    prepare_user_data(user_data_cloud_init_config);
    config->factory->configure(name, meta_data_cloud_init_config, vendor_data_cloud_init_config);
    auto vm_desc = to_machine_desc(request, name, vm_image, meta_data_cloud_init_config, user_data_cloud_init_config,
                                   vendor_data_cloud_init_config);

    config->factory->prepare_instance_image(vm_image, vm_desc);

    vm_instances[name] = config->factory->create_virtual_machine(vm_desc, *this);
    vm_instance_specs[name] = {vm_desc.num_cores, vm_desc.mem_size, vm_desc.disk_space, {}};
    persist_instances();

    reply.set_create_message("Starting " + name);
    server->Write(reply);

    auto& vm = vm_instances[name];
    vm->start();
    vm->wait_until_ssh_up(std::chrono::minutes(5));

    reply.set_create_message("Waiting for cloud-init to complete");
    server->Write(reply);
    wait_until_cloud_init_finished(vm->ssh_hostname(), vm->ssh_port(), *config->ssh_key_provider,
                                   std::chrono::minutes(5));

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

grpc::Status mp::Daemon::empty_trash(grpc::ServerContext* context, const EmptyTrashRequest* request,
                                     EmptyTrashReply* response) // clang-format off
try // clang-format on
{
    std::vector<decltype(vm_instance_trash)::key_type> keys_to_delete;
    for (auto& trash : vm_instance_trash)
    {
        const auto& name = trash.first;
        config->factory->remove_resources_for(name);
        config->vault->remove(name);
        keys_to_delete.push_back(name);
    }

    for (auto const& key : keys_to_delete)
    {
        vm_instance_trash.erase(key);
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
        auto vm_images_info =
            config->image_host->all_info_for({"", request->search_string(), false, request->remote_name()});

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
    std::string error_messages;
    for (const auto& name : request->instance_name())
    {
        auto it = vm_instances.find(name);
        bool in_trash{false};
        if (it == vm_instances.end())
        {
            it = vm_instance_trash.find(name);
            if (it == vm_instance_trash.end())
            {
                error_messages.append("instance \"" + name + "\" does not exist\n");
                continue;
            }
            in_trash = true;
        }

        auto info = response->add_info();
        auto vm_image = fetch_image_for(name, config->factory->fetch_type(), *config->vault);
        auto& vm = it->second;
        info->set_name(name);
        if (in_trash)
        {
            info->set_status(InfoReply::TRASHED);
        }
        else
        {
            auto status_for = [](mp::VirtualMachine::State state) {
                switch (state)
                {
                case mp::VirtualMachine::State::running:
                    return InfoReply::RUNNING;
                default:
                    return InfoReply::STOPPED;
                }
            };
            info->set_status(status_for(vm->current_state()));
        }

        auto vm_image_info = config->image_host->info_for_full_hash(vm_image.id);
        info->set_image_release(vm_image_info.release_title.toStdString());
        info->set_id(vm_image.id);

        auto vm_specs = vm_instance_specs[name];

        auto mount_info = info->mutable_mount_info();

        mount_info->set_longest_path_len(0);

        for (const auto& mount : vm_specs.mounts)
        {
            if (mount.first.size() > mount_info->longest_path_len())
            {
                mount_info->set_longest_path_len(mount.first.size());
            }

            auto entry = mount_info->add_mount_paths();
            entry->set_source_path(mount.second.source_path);
            entry->set_target_path(mount.first);
        }

        if (vm->current_state() == mp::VirtualMachine::State::running)
        {
            mp::SSHSession session{vm->ssh_hostname(), vm->ssh_port(), *config->ssh_key_provider};

            auto ssh_process = session.exec({"cat /proc/loadavg | cut -d ' ' -f1-3"});
            auto output = ssh_process.read_std_output();
            // Remove trailing newline
            output.pop_back();
            info->set_load(output);

            ssh_process = session.exec({"free -h | grep Mem | awk '{printf \"%s out of %s\", $3, $2}'"});
            output = ssh_process.read_std_output();
            info->set_memory_usage(output);

            ssh_process = session.exec({"df -h | grep -w /dev/sda1 | awk '{printf \"%s out of %s\", $3, $2}'"});
            output = ssh_process.read_std_output();
            info->set_disk_usage(output);

            ssh_process = session.exec({"lsb_release -ds"});
            output = ssh_process.read_std_output();
            // Remove trailing newline
            output.pop_back();
            info->set_current_release(output);

            info->set_ipv4(vm->ipv4());
        }
    }
    if (!error_messages.empty())
    {
        // Remove the last trailing new line.
        error_messages.pop_back();
        return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "The following errors occurred:\n" + error_messages,
                            "");
    }
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
        case mp::VirtualMachine::State::running:
            return mp::ListVMInstance::RUNNING;
        default:
            return mp::ListVMInstance::STOPPED;
        }
    };

    for (const auto& instance : vm_instances)
    {
        const auto& name = instance.first;
        const auto& vm = instance.second;
        auto entry = response->add_instances();
        entry->set_name(name);
        entry->set_status(status_for(vm->current_state()));

        // FIXME: Set the release to the cached current version when supported
        auto vm_image = fetch_image_for(name, config->factory->fetch_type(), *config->vault);
        auto vm_image_info = config->image_host->info_for_full_hash(vm_image.id);
        entry->set_current_release("Ubuntu " + vm_image_info.release_title.toStdString());

        if (vm->current_state() == mp::VirtualMachine::State::running)
            entry->set_ipv4(vm->ipv4());
    }

    for (const auto& instance : vm_instance_trash)
    {
        const auto& name = instance.first;
        auto entry = response->add_instances();
        entry->set_name(name);
        entry->set_status(mp::ListVMInstance::TRASHED);
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
    std::string error_string{"The following errors occured:\n"};
    bool failures = false;

    QFileInfo source_dir(QString::fromStdString(request->source_path()));
    if (!source_dir.exists())
    {
        return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                            "source \"" + request->source_path() + "\" does not exist", "");
    }

    if (!source_dir.isDir())
    {
        return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                            "source \"" + request->source_path() + "\" is not a directory", "");
    }

    if (!source_dir.isReadable())
    {
        return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                            "source \"" + request->source_path() + "\" is not readable", "");
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

    for (const auto& path_entry : request->target_paths())
    {
        const auto name = path_entry.instance_name();
        auto it = vm_instances.find(name);
        if (it == vm_instances.end())
        {
            error_string.append("instance \"" + name + "\" does not exist\n");
            failures = true;
            continue;
        }

        auto target_path = path_entry.target_path();

        if (mp::utils::invalid_target_path(QString::fromStdString(target_path)))
        {
            error_string.append("Unable to mount to \"" + target_path + "\".\n");
            failures = true;
            continue;
        }

        auto entry = mount_threads.find(name);

        if (entry != mount_threads.end() && entry->second.find(target_path) != entry->second.end())
        {
            error_string.append("\"" + name + ":" + target_path + "\" is already mounted.\n");
            failures = true;
            continue;
        }

        auto& vm = it->second;

        if (vm->current_state() == mp::VirtualMachine::State::running)
        {
            auto status = start_mount(vm, name, request->source_path(), target_path, gid_map, uid_map);

            if (!status.empty())
            {
                error_string.append(status);
                failures = true;
                continue;
            }
        }

        auto& vm_specs = vm_instance_specs[name];
        if (vm_specs.mounts.find(target_path) != vm_specs.mounts.end())
        {
            error_string.append("There is already a mount defined for \"" + name + ":" + target_path + "\".\n");
            failures = true;
            continue;
        }

        VMMount mount{request->source_path(), gid_map, uid_map};
        vm_specs.mounts[target_path] = mount;
    }

    persist_instances();

    if (failures)
    {
        // Remove trailing newline due to grpc adding one of it's own
        error_string.pop_back();

        return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, error_string, "");
    }
    else
    {
        return grpc::Status::OK;
    }
}
catch (const mp::SSHFSMissingError& e)
{
    return handle_mount_error(e.name());
}
catch (const std::exception& e)
{
    return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, e.what(), "");
}

grpc::Status mp::Daemon::recover(grpc::ServerContext* context, const RecoverRequest* request,
                                 RecoverReply* response) // clang-format off
try // clang-format on
{
    std::string error_messages;
    std::vector<decltype(vm_instance_trash)::key_type> instances_to_recover;
    for (const auto& name : request->instance_name())
    {
        auto it = vm_instance_trash.find(name);
        if (it == vm_instance_trash.end())
        {
            it = vm_instances.find(name);
            if (it == vm_instances.end())
                error_messages.append("instance \"" + name + "\" does not exist\n");
            else
                error_messages.append("instance \"" + name + "\" has not been deleted\n");
            continue;
        }
        instances_to_recover.push_back(name);
    }

    if (!error_messages.empty())
    {
        // Remove the last trailing new line.
        error_messages.pop_back();
        return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "The following errors occurred:\n" + error_messages,
                            "");
    }

    if (instances_to_recover.empty())
    {
        for (auto& pair : vm_instance_trash)
            instances_to_recover.push_back(pair.first);
    }

    for (const auto& name : instances_to_recover)
    {
        auto it = vm_instance_trash.find(name);
        it->second->shutdown();
        vm_instances[name] = std::move(it->second);
        vm_instance_trash.erase(name);
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
    const auto name = request->instance_name();
    auto it = vm_instances.find(name);
    if (it == vm_instances.end())
    {
        return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "instance \"" + name + "\" does not exist", "");
    }

    if (it->second->current_state() != mp::VirtualMachine::State::running)
    {
        return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, "instance \"" + name + "\" is not running", "");
    }

    response->set_host(it->second->ssh_hostname());
    response->set_port(it->second->ssh_port());
    response->set_priv_key_base64(config->ssh_key_provider->private_key_as_base64());
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

    std::string error_messages;
    std::vector<decltype(vm_instances)::key_type> vms;
    for (const auto& name : request->instance_name())
    {
        auto it = vm_instances.find(name);
        if (it == vm_instances.end())
        {
            it = vm_instance_trash.find(name);
            if (it == vm_instance_trash.end())
            {
                mp::StartError start_error;
                start_error.set_error_code(mp::StartError::DOES_NOT_EXIST);
                start_error.set_instance_name(name);
                return grpc::Status(grpc::StatusCode::ABORTED, "instance \"" + name + "\" does not exist",
                                    start_error.SerializeAsString());
            }
            else
            {
                mp::StartError start_error;
                start_error.set_error_code(mp::StartError::INSTANCE_DELETED);
                start_error.set_instance_name(name);
                return grpc::Status(grpc::StatusCode::ABORTED, "instance \"" + name + "\" is deleted",
                                    start_error.SerializeAsString());
            }
            continue;
        }

        if (it->second->current_state() == VirtualMachine::State::running)
            continue;

        vms.push_back(name);
    }

    if (!error_messages.empty())
    {
        // Remove the last trailing new line.
        error_messages.pop_back();
        return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "The following errors occurred:\n" + error_messages,
                            "");
    }

    if (vms.empty())
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
    std::string mount_error_messages;
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

            auto status = start_mount(vm, name, source_path, target_path, gid_map, uid_map);

            if (!status.empty())
            {
                mount_error_messages.append("Removing \"" + target_path + "\": " + status);
                invalid_mounts.push_back(target_path);
            }
        }

        update_instance_db = !invalid_mounts.empty();
        for (const auto& invalid_mount : invalid_mounts)
            mounts.erase(invalid_mount);
    }

    if (update_instance_db)
        persist_instances();

    if (!mount_error_messages.empty())
    {
        // Remove the last trailing new line.
        mount_error_messages.pop_back();
        return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                            "The following errors occurred:\n" + mount_error_messages, "");
    }

    return grpc::Status::OK;
}
catch (const mp::SSHFSMissingError& e)
{
    return handle_mount_error(e.name());
}
catch (const std::exception& e)
{
    return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, e.what(), "");
}

grpc::Status mp::Daemon::stop(grpc::ServerContext* context, const StopRequest* request,
                              StopReply* response) // clang-format off
try // clang-format on
{
    std::string error_messages;
    std::vector<decltype(vm_instances)::key_type> instances_to_stop;
    for (const auto& name : request->instance_name())
    {
        auto it = vm_instances.find(name);
        if (it == vm_instances.end())
        {
            it = vm_instance_trash.find(name);
            if (it == vm_instance_trash.end())
                error_messages.append("instance \"" + name + "\" does not exist\n");
            else
                error_messages.append("instance \"" + name + "\" is deleted\n");
            continue;
        }
        instances_to_stop.push_back(name);
    }

    if (!error_messages.empty())
    {
        // Remove the last trailing new line.
        error_messages.pop_back();
        return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "The following errors occurred:\n" + error_messages,
                            "");
    }

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

grpc::Status mp::Daemon::trash(grpc::ServerContext* context, const TrashRequest* request,
                               TrashReply* response) // clang-format off
try // clang-format on
{
    std::string error_messages;
    std::vector<decltype(vm_instances)::key_type> instances_to_trash;
    for (const auto& name : request->instance_name())
    {
        auto it = vm_instances.find(name);
        if (it == vm_instances.end())
        {
            error_messages.append("instance \"" + name + "\" does not exist\n");
            continue;
        }
        instances_to_trash.push_back(name);
    }

    if (!error_messages.empty())
    {
        // Remove the last trailing new line.
        error_messages.pop_back();
        return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "The following errors occurred:\n" + error_messages,
                            "");
    }

    if (instances_to_trash.empty())
    {
        for (auto& pair : vm_instances)
            instances_to_trash.push_back(pair.first);
    }

    const bool purge = request->purge();
    for (const auto& name : instances_to_trash)
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
            vm_instance_trash[name] = std::move(it->second);
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
    std::string error_string{"The following errors occurred:\n"};
    bool failures = false;

    for (const auto& path_entry : request->target_paths())
    {
        const auto name = path_entry.instance_name();
        auto it = vm_instances.find(name);
        if (it == vm_instances.end())
        {
            error_string.append("instance \"" + name + "\" does not exist\n");
            failures = true;
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
                    error_string.append("\"" + target_path + "\" is not mounted\n");
                    failures = true;
                }
            }

            auto erased = mounts.erase(target_path);
            if (!erased)
            {
                error_string.append("\"" + target_path + "\" not found in database\n");
            }
        }
    }

    persist_instances();

    if (failures)
    {
        // Remove trailing newline due to grpc adding one of it's own
        error_string.pop_back();

        return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, error_string, "");
    }
    else
    {
        return grpc::Status::OK;
    }
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

void mp::Daemon::persist_instances()
{
    auto vm_spec_to_json = [](const mp::VMSpecs& specs) -> QJsonObject {
        QJsonObject json;
        json.insert("num_cores", specs.num_cores);
        json.insert("mem_size", QString::fromStdString(specs.mem_size));
        json.insert("disk_space", QString::fromStdString(specs.disk_space));

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

std::string mp::Daemon::start_mount(const VirtualMachine::UPtr& vm, const std::string& name, const std::string& source_path,
                                    const std::string& target_path, const std::unordered_map<int, int>& gid_map,
                                    const std::unordered_map<int, int>& uid_map)
{
    try
    {
        auto& key_provider = *config->ssh_key_provider;
        auto session_factory = [&vm, &key_provider]() -> SSHSession {
            return {vm->ssh_hostname(), vm->ssh_port(), key_provider};
        };

        auto sshfs_mount =
            std::make_unique<mp::SshfsMount>(session_factory, QString::fromStdString(source_path),
                                             QString::fromStdString(target_path), gid_map, uid_map, config->cout);

        sshfs_mount->run();

        QObject::connect(sshfs_mount.get(), &SshfsMount::finished, this, [this, name, target_path]() {
            mount_threads[name].erase(target_path);
        });

        mount_threads[name][target_path] = std::move(sshfs_mount);
    }
    catch (const mp::SSHFSMissingError& e)
    {
        throw mp::SSHFSMissingError(name);
    }
    catch (const std::exception& e)
    {
        return e.what();
    }

    return {};
}
