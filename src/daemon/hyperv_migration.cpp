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

#include <hyperv/hyperv_disk_layout.h>
#include <hyperv/hyperv_migrator.h>
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

bool has_ownership(const fs::path& dir)
{
    try
    {
        return mhv::HCSOwnership::load(dir).has_value();
    }
    catch (const std::exception& error)
    {
        mpl::warn(log_category,
                  "Ignoring malformed HCS ownership marker under '{}': {}",
                  dir,
                  error.what());
        return false;
    }
}

bool remove_tree(const fs::path& dir)
{
    std::error_code error;
    fs::remove_all(dir, error);
    if (error)
        mpl::warn(log_category,
                  "Could not remove migration target directory '{}': {}",
                  dir,
                  error.message());
    return !error;
}

std::optional<mhv::MigrationTransactionManifest> try_load_manifest(const fs::path& dir)
{
    try
    {
        return mhv::MigrationTransactionManifest::load(dir);
    }
    catch (const std::exception& error)
    {
        mpl::warn(log_category,
                  "Ignoring malformed migration manifest under '{}': {}",
                  dir,
                  error.what());
        return std::nullopt;
    }
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

multipass::hyperv::HyperVMigrationTargetRecords::HyperVMigrationTargetRecords(const Path& data_dir)
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

    recover();
}

void multipass::hyperv::HyperVMigrationTargetRecords::recover()
{
    std::error_code error;
    auto iterator = MP_FILEOPS.dir_iterator(instances_root, error);
    if (error || !iterator)
        return;

    std::vector<fs::path> directories;
    while (iterator->hasNext())
    {
        const auto path = iterator->next().path();
        const auto name = path.filename().string();
        if (name == "." || name == "..")
            continue;

        std::error_code dir_error;
        if (MP_FILEOPS.is_directory(path, dir_error) && !dir_error)
            directories.push_back(path);
    }

    for (const auto& path : directories)
    {
        const auto name = path.filename().string();
        const auto manifest = try_load_manifest(path);

        if (name.starts_with(mhv::migration_staging_prefix))
        {
            if (manifest && manifest->phase == MigrationTransactionManifest::staged_phase_name)
            {
                mpl::info(log_category, "Removing leftover migration staging directory '{}'", name);
                remove_tree(path);
            }
            continue;
        }

        if (!manifest || manifest->vm_name != name)
            continue;

        if (has_vm_record(name))
        {
            if (manifest->phase != MigrationTransactionManifest::prepared_phase_name ||
                !has_image_record(name) || !has_ownership(path))
            {
                mpl::warn(log_category,
                          "Committed target '{}' has incomplete image or ownership state; leaving "
                          "its migration manifest in place for inspection",
                          name);
                continue;
            }

            std::error_code remove_error;
            if (MP_FILEOPS.remove(path / MigrationTransactionManifest::filename, remove_error) &&
                !remove_error)
                mpl::info(log_category, "Finalized committed migration target '{}'", name);
            else
                mpl::warn(log_category, "Could not remove stale migration manifest for '{}'", name);
        }
        else
        {
            if (target_image_records.erase(name) > 0)
                persist_records(target_image_records, target_image_db);

            if (remove_tree(path))
                mpl::info(log_category, "Removed pre-commit migration orphan '{}'", name);
        }
    }

    std::vector<std::string> record_only_orphans;
    for (const auto& [name, _] : target_image_records)
    {
        std::error_code exists_error;
        if (!has_vm_record(name) && !MP_FILEOPS.exists(instance_dir(name), exists_error) &&
            !exists_error)
            record_only_orphans.push_back(name);
    }

    for (const auto& name : record_only_orphans)
    {
        target_image_records.erase(name);
        persist_records(target_image_records, target_image_db);
        mpl::info(log_category, "Removed orphan migration image record '{}'", name);
    }
}

bool multipass::hyperv::HyperVMigrationTargetRecords::target_exists(const std::string& name) const
{
    const auto target_dir = instance_dir(name);
    std::error_code error;
    return (MP_FILEOPS.exists(target_dir, error) && !error) || has_vm_record(name) ||
           has_image_record(name);
}

multipass::VaultRecord multipass::hyperv::HyperVMigrationTargetRecords::source_image_record(
    const std::string& name) const
{
    const auto* record = source_image_records.if_contains(name);
    if (!record)
        throw InstanceMigrationError{fmt::format("source image record for '{}' is missing", name)};

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

void multipass::hyperv::HyperVMigrationTargetRecords::commit(const std::string& name,
                                                             const VMSpecs& spec,
                                                             VaultRecord image_record)
{
    if (has_vm_record(name) || has_image_record(name))
        throw MigrationAbortError{
            fmt::format("target records for '{}' appeared during migration", name)};

    const auto target_dir = instance_dir(name);
    std::optional<MigrationTransactionManifest> manifest;
    std::optional<HCSOwnership> ownership;
    try
    {
        manifest = MigrationTransactionManifest::load(target_dir);
        ownership = HCSOwnership::load(target_dir);
    }
    catch (const std::exception& error)
    {
        throw MigrationAbortError{
            fmt::format("prepared target '{}' has invalid transaction metadata: {}",
                        name,
                        error.what())};
    }

    if (!manifest || manifest->phase != MigrationTransactionManifest::prepared_phase_name ||
        manifest->vm_name != name || !ownership)
        throw MigrationAbortError{
            fmt::format("prepared target '{}' has incomplete transaction metadata", name)};

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

bool multipass::hyperv::HyperVMigrationTargetRecords::has_vm_record(const std::string& name) const
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

    auto target_interfaces = translate_extra_interfaces(source_interfaces, *source_networks);
    for (auto& target_interface : target_interfaces)
    {
        auto& factory = target_factory();
        const auto existing_networks = factory.networks();
        std::vector<NetworkInterface> one_interface{target_interface};
        factory.prepare_networking(one_interface);
        target_interface = std::move(one_interface.front());

        const auto already_existed = std::ranges::any_of(
            existing_networks,
            [&target_interface](const auto& network) { return network.id == target_interface.id; });
        if (!already_existed)
            created_network_guids.push_back(utils::make_uuid(target_interface.id));
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

multipass::hyperv::InstanceMigrationResult multipass::hyperv::DaemonHyperVInstanceMigrator::migrate(
    const std::string& name,
    MigrationProgress& progress)
{
    const auto spec_it = specs.find(name);
    if (spec_it == specs.end())
        throw InstanceMigrationError{fmt::format("source VM record for '{}' is missing", name)};

    const auto& source_spec = spec_it->second;
    if (source_spec.deleted || deleted_instances.contains(name))
        return "instance is deleted";

    const auto vm_it = operative_instances.find(name);
    if (vm_it == operative_instances.end() || !vm_it->second)
        return "instance is unavailable";

    const auto state = vm_it->second->current_state();
    if (state != VirtualMachine::State::off && state != VirtualMachine::State::stopped)
        return fmt::format("instance is {} and needs to be stopped", state_name(state));

    if (target_records.target_exists(name))
        return "name already taken by a hyperv_api instance";

    try
    {
        std::vector<std::string> created_network_guids;
        auto cleanup_networks = sg::make_scope_guard([&created_network_guids]() noexcept {
            cleanup_created_networks(created_network_guids);
        });

        progress.phase(name, "Preparing networking");
        auto target_spec = source_spec;
        target_spec.extra_interfaces = translated_interfaces(source_spec.extra_interfaces,
                                                             created_network_guids);
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

        progress.phase(name, "inspecting source disk layout");
        auto layout = resolve_legacy_disk_layout(name, *vm_it->second);
        for (auto& snapshot : layout.snapshots)
            snapshot.extra_interfaces = translated_interfaces(snapshot.extra_interfaces,
                                                              created_network_guids);

        const auto ownership = migrate_retained_copy(
            layout,
            MP_PLATFORM.qstr_to_path(source_instance_dir.absolutePath()),
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
        return std::nullopt;
    }
    catch (const MigrationAbortError&)
    {
        throw;
    }
    catch (const std::exception& error)
    {
        throw InstanceMigrationError{error.what()};
    }
}
