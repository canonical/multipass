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

#include "hyperv_target_store.h"

#include "hyperv_target_transaction.h"

#include <hyperv_api/hcs_ownership.h>

#include <multipass/file_ops.h>
#include <multipass/logging/log.h>
#include <multipass/top_catch_all.h>

#include <fmt/format.h>
#include <fmt/std.h>

#include <optional>
#include <string_view>
#include <system_error>
#include <vector>

namespace
{
namespace fs = std::filesystem;
namespace mhv = multipass::hyperv;
namespace mpl = multipass::logging;

constexpr auto log_category = "Hyper-V migration target store";

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
                  dir.string(),
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
                  dir.string(),
                  error.message());
    return !error;
}

bool is_staging_name(std::string_view name)
{
    return name.starts_with(mhv::HyperVTargetStore::staging_prefix);
}

// Cleanup is only ever driven by a *valid* migration manifest. A manifest that cannot be
// parsed/validated is treated as "no manifest", so the enclosing directory is left
// strictly untouched instead of aborting the whole recovery sweep.
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
                  dir.string(),
                  error.what());
        return std::nullopt;
    }
}
} // namespace

multipass::hyperv::TargetRecoveryReport multipass::hyperv::HyperVTargetStore::recover(
    const fs::path& instances_root,
    const TargetRecordPredicate& has_committed_vm_record,
    const TargetRecordPredicate& has_committed_image_record)
{
    TargetRecoveryReport report;

    std::error_code error;
    if (!MP_FILEOPS.exists(instances_root, error) || error)
        return report;

    auto iterator = MP_FILEOPS.dir_iterator(instances_root, error);
    if (error || !iterator)
        return report;

    std::vector<fs::path> directories;
    while (iterator->hasNext())
    {
        const auto& entry = iterator->next();
        const auto path = entry.path();
        const auto name = path.filename().string();
        if (name == "." || name == "..")
            continue;

        std::error_code dir_error;
        if (!MP_FILEOPS.is_directory(path, dir_error) || dir_error)
            continue;
        directories.push_back(path);
    }

    for (const auto& path : directories)
    {
        const auto name = path.filename().string();
        const auto manifest = try_load_manifest(path);

        // A staging name alone is not proof of ownership.
        if (is_staging_name(name))
        {
            if (!manifest ||
                manifest->phase != MigrationTransactionManifest::staged_phase_name)
                continue;

            mpl::info(log_category, "Removing leftover migration staging directory '{}'", name);
            remove_tree(path);
            continue;
        }

        // Only manifest-owned final directories are eligible for cleanup; everything else
        // (normal instances, already-finalized targets) is left strictly untouched.
        if (!manifest || manifest->vm_name != name)
            continue;

        if (has_committed_vm_record(name))
        {
            // The visible target is committed. Preserve it, validate its ownership, and
            // drop the stale manifest so future sweeps ignore it.
            if (manifest->phase != MigrationTransactionManifest::prepared_phase_name ||
                !has_committed_image_record(name) || !has_ownership(path))
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
            {
                report.finalized.push_back(name);
                mpl::info(log_category, "Finalized committed migration target '{}'", name);
            }
            else
            {
                mpl::warn(log_category,
                          "Could not remove stale migration manifest for '{}'",
                          name);
            }
        }
        else
        {
            // No VM DB entry: the target never crossed the commit boundary. Remove the
            // pre-commit directory and report the orphan so its image record can be dropped.
            if (remove_tree(path))
            {
                report.removed_precommit.push_back(name);
                mpl::info(log_category, "Removed pre-commit migration orphan '{}'", name);
            }
        }
    }

    return report;
}

bool multipass::hyperv::HyperVTargetStore::target_exists(const fs::path& instances_root,
                                                         const std::string& name,
                                                         const TargetRecordPredicate& has_record)
{
    const auto instance_dir = instances_root / name;

    std::error_code error;
    if (MP_FILEOPS.exists(instance_dir, error) && !error)
        return true;

    if (has_ownership(instance_dir))
        return true;

    return has_record(name);
}
