/*
 * Copyright (C) Canonical, Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "hyperv_migration.h"

#include <hyperv/hyperv_migrator.h>
#include <hyperv/hyperv_target_store.h>
#include <hyperv/hyperv_target_transaction.h>
#include <hyperv_api/hcn/hyperv_hcn_wrapper.h>
#include <hyperv_api/hcs_ownership.h>
#include <hyperv_api/hcs_virtual_machine_factory.h>

#include <multipass/constants.h>
#include <multipass/file_ops.h>
#include <multipass/json_utils.h>
#include <multipass/logging/log.h>
#include <multipass/platform.h>
#include <multipass/top_catch_all.h>
#include <multipass/utils.h>
#include <multipass/virtual_machine_description.h>
#include <multipass/virtual_machine_factory.h>

#include <QFileInfo>

#include <fmt/format.h>
#include <fmt/std.h>
#include <scope_guard.hpp>

#include <algorithm>
#include <system_error>

namespace
{
namespace fs = std::filesystem;
namespace mhv = multipass::hyperv;
namespace mpl = multipass::logging;

constexpr auto log_category = "Hyper-V migration records";
constexpr auto target_backend = "hyperv_api";
constexpr auto vm_db_filename = "multipassd-vm-instances.json";
constexpr auto image_db_filename = "multipassd-instance-image-records.json";

fs::path as_path(const multipass::Path& path)
{
    return MP_PLATFORM.qstr_to_path(path);
}

std::string state_name(multipass::VirtualMachine::State state)
{
    using State = multipass::VirtualMachine::State;
    switch (state)
    {
    case State::off:
    case State::stopped:
        return "stopped";
    case State::running:
        return "running";
    case State::suspended:
        return "suspended";
    case State::starting:
        return "starting";
    case State::restarting:
        return "restarting";
    case State::delayed_shutdown:
        return "delayed shutdown";
    case State::unknown:
        return "unknown";
    case State::unavailable:
        return "unavailable";
    }
    return "unsupported";
}
} // namespace

multipass::hyperv::HyperVMigrationTargetRecords::HyperVMigrationTargetRecords(
    const Path& data_dir)
    : source_image_db{as_path(data_dir) / "vault" / image_db_filename},
      target_root{as_path(data_dir) / target_backend},
      target_vm_db{target_root / vm_db_filename},
      target_image_db{target_root / "vault" / image_db_filename},
      instances_root{target_root / "vault" / "instances"},
      source_image_records{load_records(source_image_db)},
      target_vm_records{load_records(target_vm_db)},
      target_image_records{load_records(target_image_db)}
{
}

boost::json::object multipass::hyperv::HyperVMigrationTargetRecords::load_records(
    const fs::path& path)
{
    const auto contents = MP_FILEOPS.try_read_file(path);
    if (!contents || contents->empty())
        return {};

    auto records = boost::json::parse(*contents);
    if (!records.is_object())
        throw std::runtime_error{
            fmt::format("Migration record file '{}' does not contain a JSON object", path)};
    return std::move(records.as_object());
}

void multipass::hyperv::HyperVMigrationTargetRecords::persist_records(
    const boost::json::object& records,
    const fs::path& path)
{
    MP_FILEOPS.write_transactionally(path, multipass::pretty_print(records));
}

void multipass::hyperv::HyperVMigrationTargetRecords::require_writable_location(
    const fs::path& path)
{
    auto existing = path;
    while (!existing.empty() && !MP_FILEOPS.exists(existing))
        existing = existing.parent_path();

    if (existing.empty())
        throw std::runtime_error{
            fmt::format("Could not find an existing parent for migration path '{}'", path)};

    const QFileInfo info{QString::fromStdWString(existing.wstring())};
    if (!info.isDir() || !info.isWritable())
        throw std::runtime_error{
            fmt::format("Migration target location '{}' is not writable", existing)};
}

void multipass::hyperv::HyperVMigrationTargetRecords::preflight() const
{
    require_writable_location(target_root);

    for (const auto& database : {target_vm_db, target_image_db})
    {
        if (!MP_FILEOPS.exists(database))
            continue;

        const QFileInfo info{QString::fromStdWString(database.wstring())};
        if (!info.isReadable() || !info.isWritable())
            throw std::runtime_error{
                fmt::format("Migration target database '{}' is not readable and writable",
                            database)};
    }
}

void multipass::hyperv::HyperVMigrationTargetRecords::prepare()
{
    std::error_code error;
    if (!MP_FILEOPS.create_directories(instances_root, error) && error)
        throw MigrationAbortError{
            fmt::format("Could not create target instances directory '{}': {}",
                        instances_root,
                        error.message())};

    const auto recovery = HyperVTargetStore::recover(
        instances_root,
        [this](const auto& name) { return has_vm_record(name); },
        [this](const auto& name) { return has_image_record(name); });

    auto records_changed = false;
    for (const auto& name : recovery.removed_precommit)
        records_changed = target_image_records.erase(name) > 0 || records_changed;

    if (records_changed)
        persist_records(target_image_records, target_image_db);
}

bool multipass::hyperv::HyperVMigrationTargetRecords::target_exists(
    const std::string& name) const
{
    return HyperVTargetStore::target_exists(instances_root, name, [this](const auto& candidate) {
        return has_vm_record(candidate) || has_image_record(candidate);
    });
}

multipass::VaultRecord multipass::hyperv::HyperVMigrationTargetRecords::source_image_record(
    const std::string& name) const
{
    const auto* record = source_image_records.if_contains(name);
    if (!record)
        throw InstanceMigrationError{
            fmt::format("source image record for '{}' is missing", name)};

    try
    {
        return boost::json::value_to<VaultRecord>(*record);
    }
    catch (const std::exception& error)
    {
        throw InstanceMigrationError{
            fmt::format("source image record for '{}' is invalid: {}", name, error.what())};
    }
}

std::filesystem::path multipass::hyperv::HyperVMigrationTargetRecords::instance_dir(
    const std::string& name) const
{
    return instances_root / name;
}

void multipass::hyperv::HyperVMigrationTargetRecords::commit(
    const std::string& name,
    const VMSpecs& spec,
    VaultRecord image_record)
{
    if (has_vm_record(name) || has_image_record(name))
        throw MigrationAbortError{
            fmt::format("target records for '{}' appeared during migration", name)};

    const auto target_dir = instance_dir(name);
    const auto manifest = MigrationTransactionManifest::load(target_dir);
    const auto ownership = HCSOwnership::load(target_dir);
    if (!manifest || manifest->phase != MigrationTransactionManifest::prepared_phase_name ||
        manifest->vm_name != name || !ownership)
        throw MigrationAbortError{
            fmt::format("prepared target '{}' has incomplete transaction metadata", name)};
    if (MP_FILEOPS.weakly_canonical(manifest->active_disk) !=
            MP_FILEOPS.weakly_canonical(ownership->active_disk) ||
        MP_FILEOPS.weakly_canonical(manifest->state_file_stem) !=
            MP_FILEOPS.weakly_canonical(ownership->state_file_stem))
        throw MigrationAbortError{
            fmt::format("prepared target '{}' has inconsistent ownership metadata", name)};

    image_record.image.image_path = ownership->active_disk;

    target_image_records[name] = boost::json::value_from(image_record);
    try
    {
        persist_records(target_image_records, target_image_db);
    }
    catch (const std::exception& error)
    {
        throw MigrationAbortError{
            fmt::format("could not persist target image record for '{}': {}", name, error.what())};
    }

    target_vm_records[name] = boost::json::value_from(spec);
    try
    {
        persist_records(target_vm_records, target_vm_db);
    }
    catch (const std::exception& error)
    {
        throw MigrationAbortError{
            fmt::format("could not persist target VM record for '{}': {}", name, error.what())};
    }

    std::error_code remove_error;
    if (!MP_FILEOPS.remove(target_dir / MigrationTransactionManifest::filename, remove_error) ||
        remove_error)
        mpl::warn(log_category,
                  "Could not remove committed migration manifest for '{}': {}",
                  name,
                  remove_error.message());
}

bool multipass::hyperv::HyperVMigrationTargetRecords::has_vm_record(
    const std::string& name) const
{
    return target_vm_records.contains(name);
}

bool multipass::hyperv::HyperVMigrationTargetRecords::has_image_record(
    const std::string& name) const
{
    return target_image_records.contains(name);
}

multipass::hyperv::DaemonHyperVInstanceMigrator::DaemonHyperVInstanceMigrator(
    const std::unordered_map<std::string, VMSpecs>& specs,
    const InstanceTable& operative_instances,
    const InstanceTable& deleted_instances,
    VirtualMachineFactory& source_factory,
    const SSHKeyProvider& key_provider,
    AvailabilityZoneManager& az_manager,
    const Path& data_dir,
    HyperVMigrationTargetRecords& target_records)
    : specs{specs},
      operative_instances{operative_instances},
      deleted_instances{deleted_instances},
      source_factory{source_factory},
      key_provider{key_provider},
      az_manager{az_manager},
      data_dir{data_dir},
      target_records{target_records}
{
}

multipass::hyperv::DaemonHyperVInstanceMigrator::~DaemonHyperVInstanceMigrator() = default;

std::vector<std::string> multipass::hyperv::DaemonHyperVInstanceMigrator::source_names()
{
    std::vector<std::string> names;
    names.reserve(specs.size());
    for (const auto& [name, _] : specs)
        names.push_back(name);
    return names;
}

std::vector<multipass::NetworkInterface>
multipass::hyperv::DaemonHyperVInstanceMigrator::translated_interfaces(
    const std::vector<NetworkInterface>& source_interfaces,
    std::vector<std::string>& created_network_guids)
{
    if (source_interfaces.empty())
        return {};

    if (!source_networks)
        source_networks = source_factory.networks();

    const auto translated = translate_extra_interfaces(source_interfaces, *source_networks);
    std::vector<NetworkInterface> target_interfaces;
    target_interfaces.reserve(translated.size());
    for (const auto& interface : translated)
    {
        auto target_interface = NetworkInterface{.id = interface.adapter_id,
                                                 .mac_address = interface.mac_address,
                                                 .auto_mode = interface.auto_mode};
        auto existing_networks = target_factory().networks();
        std::vector<NetworkInterface> one_interface{target_interface};
        target_factory().prepare_networking(one_interface);
        target_interface = std::move(one_interface.front());

        const auto already_existed = std::ranges::any_of(
            existing_networks,
            [&target_interface](const auto& network) {
                return network.id == target_interface.id;
            });
        if (!already_existed)
            created_network_guids.push_back(utils::make_uuid(target_interface.id));

        target_interfaces.push_back(std::move(target_interface));
    }

    return target_interfaces;
}

void multipass::hyperv::DaemonHyperVInstanceMigrator::cleanup_created_networks(
    const std::vector<std::string>& network_guids) noexcept
{
    for (auto it = network_guids.rbegin(); it != network_guids.rend(); ++it)
        multipass::top_catch_all(log_category, [&guid = *it] {
            if (const auto result = hcn::HCN().delete_network(guid); !result)
                mpl::warn(log_category,
                          "Could not remove migration-created HCN network '{}': {}",
                          guid,
                          result);
        });
}

multipass::hyperv::HCSVirtualMachineFactory&
multipass::hyperv::DaemonHyperVInstanceMigrator::target_factory()
{
    if (!hcs_factory)
        hcs_factory = std::make_unique<HCSVirtualMachineFactory>(data_dir, az_manager);
    return *hcs_factory;
}

multipass::hyperv::InstanceMigrationResult
multipass::hyperv::DaemonHyperVInstanceMigrator::migrate(const std::string& name,
                                                         MigrationProgress& progress)
{
    const auto spec_it = specs.find(name);
    if (spec_it == specs.end())
        throw InstanceMigrationError{fmt::format("source VM record for '{}' is missing", name)};

    const auto& source_spec = spec_it->second;
    if (source_spec.deleted || deleted_instances.contains(name))
        return {InstanceMigrationOutcome::skipped, "instance is deleted"};

    const auto vm_it = operative_instances.find(name);
    if (vm_it == operative_instances.end() || !vm_it->second)
        return {InstanceMigrationOutcome::skipped, "instance is unavailable"};

    const auto state = vm_it->second->current_state();
    if (state != VirtualMachine::State::off && state != VirtualMachine::State::stopped)
        return {InstanceMigrationOutcome::skipped,
                fmt::format("instance is {} and needs to be stopped", state_name(state))};

    if (target_records.target_exists(name))
        return {InstanceMigrationOutcome::skipped,
                "name already taken by a hyperv_api instance"};

    try
    {
        std::vector<std::string> created_network_guids;
        auto cleanup_networks = sg::make_scope_guard(
            [&created_network_guids]() noexcept {
                cleanup_created_networks(created_network_guids);
            });

        progress.phase(name, "Preparing networking");
        auto target_spec = source_spec;
        target_spec.extra_interfaces =
            translated_interfaces(source_spec.extra_interfaces, created_network_guids);
        auto image_record = target_records.source_image_record(name);

        const auto source_instance_dir = vm_it->second->instance_directory();
        VirtualMachineDescription description{target_spec.num_cores,
                                              target_spec.mem_size,
                                              target_spec.disk_space,
                                              name,
                                              target_spec.zone,
                                              target_spec.default_mac_address,
                                              target_spec.extra_interfaces,
                                              target_spec.ssh_username,
                                              image_record.image,
                                              source_instance_dir.filePath(cloud_init_file_name),
                                              {},
                                              {},
                                              {},
                                              {}};

        const auto ownership = migrate_retained_copy(
            *vm_it->second,
            description,
            target_records.instance_dir(name),
            key_provider,
            az_manager.get_zone(target_spec.zone),
            target_factory().network_guid_for(target_spec.zone),
            [&progress, &name](std::string_view phase) {
                progress.phase(name, std::string{phase});
            });
        image_record.image.image_path = ownership.active_disk;

        progress.phase(name, "Committing target records");
        target_records.commit(name, target_spec, std::move(image_record));
        cleanup_networks.dismiss();
        return {InstanceMigrationOutcome::migrated, {}};
    }
    catch (const MigrationAbortError&)
    {
        throw;
    }
    catch (const InstanceMigrationError&)
    {
        throw;
    }
    catch (const std::exception& error)
    {
        throw InstanceMigrationError{error.what()};
    }
}
