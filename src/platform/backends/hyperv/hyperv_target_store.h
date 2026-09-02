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

#pragma once

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace multipass::hyperv
{
/**
 * Narrow, file-system oriented helper over the dedicated `hyperv_api` target instances
 * root. It reasons purely about migration-transaction ownership markers, HCS ownership
 * files, and final instance directories so that `Daemon::set` orchestration never has to
 * touch target JSON directly.
 *
 * The target VM database entry is the migration visibility/commit boundary. Whether that
 * entry exists for a given name is supplied by the caller through a predicate, because the
 * target VM database itself lives in the daemon/vault layer.
 */
using TargetRecordPredicate = std::function<bool(const std::string&)>;

/**
 * Outcome of a recovery sweep over the target instances root.
 */
struct TargetRecoveryReport
{
    // Names whose pre-commit (uncommitted) target directory was removed. The daemon uses
    // these to drop any matching orphan target image record.
    std::vector<std::string> removed_precommit;
    // Names whose committed target still carried a stale manifest that was removed.
    std::vector<std::string> finalized;
};

class HyperVTargetStore
{
public:
    // Reserved prefix used for uniquely named staging siblings of a final instance dir.
    static constexpr auto staging_prefix = ".migrating-";

    /**
     * Sweep @p instances_root for migration-manifest-owned artifacts and make the target
     * store consistent again after an interrupted migration:
     *   - `.migrating-*` staging siblings are always uncommitted, so they are removed;
     *   - a final instance directory that still carries a migration manifest is either
     *       * committed (its VM DB entry exists): validated, its stale manifest removed,
     *         and the visible target preserved; or
     *       * pre-commit (no VM DB entry): removed as an orphan.
     * Directories without a migration manifest are never touched.
     */
    [[nodiscard]] static TargetRecoveryReport recover(
        const std::filesystem::path& instances_root,
        const TargetRecordPredicate& has_committed_vm_record,
        const TargetRecordPredicate& has_committed_image_record);

    /**
     * True when a same-named final target artifact already exists and the source must be
     * treated as a successful skip rather than migrated: a final instance directory, an
     * HCS ownership marker, or an operative/deleted VM or image record (via @p has_record).
     */
    [[nodiscard]] static bool target_exists(const std::filesystem::path& instances_root,
                                            const std::string& name,
                                            const TargetRecordPredicate& has_record);
};
} // namespace multipass::hyperv
