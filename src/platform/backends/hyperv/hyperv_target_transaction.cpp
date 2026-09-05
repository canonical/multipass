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

#include "hyperv_target_transaction.h"

#include <hyperv_api/virtdisk/virtdisk_wrapper.h>

#include <multipass/constants.h>
#include <multipass/file_ops.h>
#include <multipass/json_utils.h>
#include <multipass/logging/log.h>
#include <multipass/top_catch_all.h>
#include <QUuid>

#include <fmt/format.h>
#include <fmt/std.h>

#include <algorithm>
#include <system_error>
#include <unordered_set>

namespace
{
namespace fs = std::filesystem;
namespace mhv = multipass::hyperv;
namespace mpl = multipass::logging;
using multipass::hyperv::virtdisk::VirtDisk;

constexpr auto log_category = "Hyper-V migration transaction";
constexpr auto snapshot_head_filename = "snapshot-head";
constexpr auto snapshot_count_filename = "snapshot-count";
// Bounded overhead reserved on the target volume for copy churn and HCS state files.
constexpr std::uintmax_t migration_overhead_bytes = 512ull * 1024 * 1024;

bool same_path(const fs::path& lhs, const fs::path& rhs)
{
    return MP_FILEOPS.weakly_canonical(lhs) == MP_FILEOPS.weakly_canonical(rhs);
}

bool path_is_within(const fs::path& path, const fs::path& root)
{
    const auto canonical_path = MP_FILEOPS.weakly_canonical(path);
    const auto canonical_root = MP_FILEOPS.weakly_canonical(root);
    auto path_it = canonical_path.begin();
    for (auto root_it = canonical_root.begin(); root_it != canonical_root.end();
         ++root_it, ++path_it)
    {
        if (path_it == canonical_path.end() || *path_it != *root_it)
            return false;
    }
    return true;
}

std::uintmax_t logical_file_length(const fs::path& path)
{
    std::error_code error;
    const auto size = MP_FILEOPS.file_size(path, error);
    if (error)
        throw std::runtime_error{
            fmt::format("Could not read the length of '{}': {}", path, error.message())};
    return size;
}

fs::path immediate_parent(const fs::path& disk)
{
    std::vector<fs::path> chain;
    if (const auto result = VirtDisk().list_virtual_disk_chain(disk, chain, 2); !result)
        throw std::runtime_error{
            fmt::format("Could not inspect virtual disk chain for '{}': {}", disk, result)};
    if (chain.empty())
        throw std::runtime_error{fmt::format("Virtual disk chain for '{}' is empty", disk)};
    if (chain.size() < 2)
        return {};
    return chain[1];
}

std::string unique_target_name(const fs::path& source, std::unordered_set<std::string>& taken)
{
    auto base = source.filename().string();
    if (taken.insert(base).second)
        return base;

    const fs::path base_path{base};
    const auto stem = base_path.stem().string();
    const auto extension = base_path.extension().string();
    for (int suffix = 1;; ++suffix)
    {
        auto candidate = fmt::format("{}-{}{}", stem, suffix, extension);
        if (taken.insert(candidate).second)
            return candidate;
    }
}
} // namespace

void multipass::hyperv::MigrationTransactionManifest::persist(const fs::path& dir) const
{
    const boost::json::object json{
        {"version", version},
        {"transaction_id", transaction_id},
        {"phase", phase},
        {"vm_name", vm_name},
    };

    MP_FILEOPS.write_transactionally(dir / filename, multipass::pretty_print(json));
}

std::optional<multipass::hyperv::MigrationTransactionManifest>
multipass::hyperv::MigrationTransactionManifest::load(const fs::path& dir)
{
    const auto contents = MP_FILEOPS.try_read_file(dir / filename);
    if (!contents)
        return std::nullopt;

    const auto json = boost::json::parse(*contents);
    const auto& object = json.as_object();
    const auto version = boost::json::value_to<int>(object.at("version"));
    if (version != current_version)
        throw std::runtime_error{
            fmt::format("Unsupported migration transaction manifest version {}", version)};

    MigrationTransactionManifest manifest{
        .version = version,
        .transaction_id = boost::json::value_to<std::string>(object.at("transaction_id")),
        .phase = boost::json::value_to<std::string>(object.at("phase")),
        .vm_name = boost::json::value_to<std::string>(object.at("vm_name")),
    };

    if (manifest.transaction_id.empty() || manifest.vm_name.empty())
        throw std::runtime_error{"Migration transaction manifest has empty identity fields"};
    if (manifest.phase != staged_phase_name && manifest.phase != prepared_phase_name)
        throw std::runtime_error{
            fmt::format("Unknown migration transaction phase '{}'", manifest.phase)};

    return manifest;
}

const std::filesystem::path& multipass::hyperv::TargetDiskMapping::target_for(
    const fs::path& source) const
{
    const auto entry = std::ranges::find_if(disks, [&source](const auto& candidate) {
        return same_path(candidate.source, source);
    });
    if (entry == disks.end())
        throw std::runtime_error{
            fmt::format("Disk '{}' is not part of the migration graph", source)};
    return entry->target;
}

multipass::hyperv::TargetMigrationTransaction::TargetMigrationTransaction(
    std::string vm_name,
    fs::path target_instance_dir)
    : vm_name{std::move(vm_name)},
      transaction_id{QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString()},
      target_instance_dir{std::move(target_instance_dir)}
{
}

multipass::hyperv::TargetMigrationTransaction::~TargetMigrationTransaction()
{
    if (!committed && staged)
        rollback();
}

multipass::hyperv::TargetDiskMapping multipass::hyperv::TargetMigrationTransaction::plan(
    const LegacyDiskLayout& layout,
    const fs::path& root) const
{
    if (layout.all_disks.empty())
        throw std::runtime_error{"Legacy disk layout has no disks to migrate"};

    TargetDiskMapping mapping{.root = root};
    std::unordered_set<std::string> taken_names;
    for (const auto& snapshot : layout.snapshots)
    {
        const auto target_name = fmt::format("{}.avhdx", snapshot.index);
        if (!taken_names.insert(target_name).second)
            throw std::runtime_error{
                fmt::format("Multiple snapshots have index {}", snapshot.index)};
    }

    for (const auto& disk : layout.all_disks)
    {
        const auto snapshot = std::ranges::find_if(
            layout.snapshots,
            [&disk](const auto& candidate) { return same_path(candidate.disk_path, disk); });
        const auto target_name = snapshot == layout.snapshots.end()
                                   ? unique_target_name(disk, taken_names)
                                   : fmt::format("{}.avhdx", snapshot->index);
        const auto target = root / target_name;
        mapping.disks.push_back({.source = disk, .target = target});
    }

    mapping.active_disk = mapping.target_for(layout.active_disk);

    for (const auto& disk : layout.all_disks)
    {
        const auto parent = immediate_parent(disk);
        if (parent.empty())
            continue;

        const auto known = std::ranges::any_of(layout.all_disks, [&parent](const auto& candidate) {
            return same_path(candidate, parent);
        });
        if (!known)
            throw std::runtime_error{fmt::format(
                "Disk '{}' has a parent '{}' outside of this instance's graph; refusing to "
                "migrate cross-instance or shared backing",
                disk,
                parent)};

        mapping.parent_links.push_back(
            {.child = mapping.target_for(disk), .parent = mapping.target_for(parent)});
    }

    mapping.snapshots.reserve(layout.snapshots.size());
    for (auto snapshot : layout.snapshots)
    {
        snapshot.disk_path = mapping.target_for(snapshot.disk_path);
        mapping.snapshots.push_back(std::move(snapshot));
    }

    return mapping;
}

void multipass::hyperv::TargetMigrationTransaction::check_space(
    const LegacyDiskLayout& layout) const
{
    std::uintmax_t required = migration_overhead_bytes;
    for (const auto& disk : layout.all_disks)
        required += logical_file_length(disk);

    std::error_code error;
    const auto info = MP_FILEOPS.space(target_instance_dir.parent_path(), error);
    if (error)
        throw std::runtime_error{fmt::format("Could not determine free space on '{}': {}",
                                             target_instance_dir.parent_path(),
                                             error.message())};
    if (info.available < required)
        throw std::runtime_error{fmt::format(
            "Insufficient space to migrate '{}': need {} bytes, only {} bytes available on '{}'",
            vm_name,
            required,
            info.available,
            target_instance_dir.parent_path())};
}

multipass::hyperv::TargetDiskMapping multipass::hyperv::TargetMigrationTransaction::stage(
    const LegacyDiskLayout& layout,
    const fs::path& source_instance_dir)
{
    if (staged)
        throw std::runtime_error{"Migration transaction has already been staged"};

    check_space(layout);

    const auto mapping = plan(layout, target_instance_dir);

    std::error_code error;
    if (!MP_FILEOPS.create_directories(target_instance_dir, error))
    {
        if (!error)
            throw std::runtime_error{fmt::format("Migration target directory already exists: '{}'",
                                                 target_instance_dir)};
        throw std::runtime_error{fmt::format("Could not create migration target directory '{}': {}",
                                             target_instance_dir,
                                             error.message())};
    }
    staged = true;

    const auto manifest = make_manifest(MigrationTransactionManifest::staged_phase_name);
    manifest.persist(target_instance_dir);

    for (const auto& entry : mapping.disks)
    {
        error.clear();
        MP_FILEOPS.copy(entry.source, entry.target, fs::copy_options::overwrite_existing, error);
        if (error)
            throw std::runtime_error{fmt::format("Could not copy disk '{}' to '{}': {}",
                                                 entry.source,
                                                 entry.target,
                                                 error.message())};
    }

    for (const auto& link : mapping.parent_links)
        if (const auto result = VirtDisk().reparent_virtual_disk(link.child, link.parent); !result)
            throw std::runtime_error{fmt::format("Could not reparent '{}' onto '{}': {}",
                                                 link.child,
                                                 link.parent,
                                                 result)};

    copy_instance_bookkeeping(source_instance_dir);
    write_snapshot_bookkeeping(mapping, source_instance_dir);

    return mapping;
}

void multipass::hyperv::TargetMigrationTransaction::verify(const TargetDiskMapping& mapping,
                                                           const LegacyDiskLayout& layout) const
{
    for (const auto& entry : mapping.disks)
    {
        if (!path_is_within(entry.target, mapping.root))
            throw std::runtime_error{
                fmt::format("Migrated disk '{}' is not target-local", entry.target)};

        const auto source_length = logical_file_length(entry.source);
        const auto target_length = logical_file_length(entry.target);
        if (source_length != target_length)
            throw std::runtime_error{
                fmt::format("Migrated disk '{}' length {} does not match source '{}' length {}",
                            entry.target,
                            target_length,
                            entry.source,
                            source_length)};
    }

    for (const auto& link : mapping.parent_links)
    {
        const auto parent = immediate_parent(link.child);
        if (parent.empty() || !same_path(parent, link.parent))
            throw std::runtime_error{
                fmt::format("Migrated disk '{}' does not reopen onto its target-local parent '{}'",
                            link.child,
                            link.parent)};
        if (!path_is_within(parent, mapping.root))
            throw std::runtime_error{
                fmt::format("Migrated disk '{}' points at a non-target-local parent '{}'",
                            link.child,
                            parent)};
    }

    if (mapping.snapshots.size() != layout.snapshots.size())
        throw std::runtime_error{"Migrated snapshot topology does not match the source"};
}

void multipass::hyperv::TargetMigrationTransaction::commit(const TargetDiskMapping& mapping)
{
    if (!staged)
        throw std::runtime_error{"Migration transaction has nothing to commit"};
    if (committed)
        throw std::runtime_error{"Migration transaction has already been committed"};

    if (!same_path(mapping.root, target_instance_dir))
        throw std::runtime_error{"Migration mapping does not belong to this transaction"};

    const auto manifest = make_manifest(MigrationTransactionManifest::prepared_phase_name);
    manifest.persist(target_instance_dir);

    committed = true;
}

void multipass::hyperv::TargetMigrationTransaction::rollback() noexcept
{
    if (committed || !staged)
        return;

    multipass::top_catch_all(log_category, [this] {
        std::error_code error;
        if (!MP_FILEOPS.exists(target_instance_dir, error))
            return;

        const auto manifest = MigrationTransactionManifest::load(target_instance_dir);
        if (manifest &&
            (manifest->transaction_id != transaction_id || manifest->vm_name != vm_name))
        {
            mpl::warn(log_category,
                      "Refusing to roll back unowned migration path '{}'",
                      target_instance_dir.string());
            return;
        }

        if (manifest && manifest->phase == MigrationTransactionManifest::prepared_phase_name)
        {
            mpl::warn(log_category,
                      "Refusing to roll back '{}' because it is already prepared",
                      target_instance_dir.string());
            return;
        }

        std::filesystem::remove_all(target_instance_dir, error);
        if (error)
            mpl::warn(log_category,
                      "Could not remove migration target directory '{}': {}",
                      target_instance_dir.string(),
                      error.message());
    });
    staged = false;
}

void multipass::hyperv::TargetMigrationTransaction::copy_instance_bookkeeping(
    const fs::path& source_instance_dir) const
{
    const auto copy_file = [this, &source_instance_dir](const auto& filename, bool required) {
        const auto source = source_instance_dir / filename;
        if (!MP_FILEOPS.exists(source))
        {
            if (required)
                throw std::runtime_error{
                    fmt::format("Required migration source file '{}' does not exist", source)};
            return;
        }

        std::error_code error;
        MP_FILEOPS.copy(source,
                        target_instance_dir / filename,
                        fs::copy_options::overwrite_existing,
                        error);
        if (error)
            throw std::runtime_error{
                fmt::format("Could not copy migration source file '{}' to '{}': {}",
                            source,
                            target_instance_dir / filename,
                            error.message())};
    };

    copy_file(multipass::cloud_init_file_name, true);
    copy_file(snapshot_count_filename, false);
}

void multipass::hyperv::TargetMigrationTransaction::write_snapshot_bookkeeping(
    const TargetDiskMapping& mapping,
    const fs::path& source_instance_dir) const
{
    for (const auto& snapshot : mapping.snapshots)
    {
        const auto filename = fmt::format("{:04}.snapshot.json", snapshot.index);
        const auto contents = MP_FILEOPS.try_read_file(source_instance_dir / filename);
        if (!contents)
            throw std::runtime_error{
                fmt::format("Could not read snapshot metadata '{}'", filename)};

        auto json = boost::json::parse(*contents);
        auto& snapshot_object = json.at("snapshot").as_object();
        if (boost::json::value_to<int>(snapshot_object.at("index")) != snapshot.index)
            throw std::runtime_error{
                fmt::format("Snapshot metadata index does not match '{}'", filename)};

        snapshot_object["extra_interfaces"] = boost::json::value_from(snapshot.extra_interfaces);
        MP_FILEOPS.write_transactionally(mapping.root / filename, multipass::pretty_print(json));
    }

    if (const auto head = MP_FILEOPS.try_read_file(source_instance_dir / snapshot_head_filename))
        MP_FILEOPS.write_transactionally(mapping.root / snapshot_head_filename, *head);
}

multipass::hyperv::MigrationTransactionManifest
multipass::hyperv::TargetMigrationTransaction::make_manifest(std::string phase) const
{
    return {
        .transaction_id = transaction_id,
        .phase = std::move(phase),
        .vm_name = vm_name,
    };
}
