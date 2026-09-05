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

#include "hyperv_disk_layout.h"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace multipass::hyperv
{
/**
 * Small versioned record persisted inside the uncommitted (and later committed) target
 * directory. It proves that the enclosing directory is owned by an in-flight - or
 * completed - migration transaction and records the phase that produced it. The next
 * phase (daemon/target DB orchestration) consumes it to reason about ownership without
 * this engine having to wire Daemon::set directly.
 */
struct MigrationTransactionManifest
{
    static constexpr int current_version = 1;
    static constexpr auto filename = "migration-transaction.json";
    static constexpr auto staged_phase_name = "staged";
    static constexpr auto prepared_phase_name = "prepared";

    int version{current_version};
    std::string transaction_id;
    std::string phase{staged_phase_name};
    std::string vm_name;

    void persist(const std::filesystem::path& dir) const;
    [[nodiscard]] static std::optional<MigrationTransactionManifest> load(
        const std::filesystem::path& dir);
};

/**
 * Deterministic mapping of every unique source disk in a legacy graph to a private,
 * target-local copy rooted under a given directory. Topology (snapshot parent/child
 * links) is preserved through @ref parent_links.
 */
struct TargetDiskMapping
{
    struct Entry
    {
        std::filesystem::path source;
        std::filesystem::path target;
    };

    struct ParentLink
    {
        std::filesystem::path child;
        std::filesystem::path parent;
    };

    std::filesystem::path root;
    std::filesystem::path active_disk;
    std::vector<Entry> disks;
    std::vector<ParentLink> parent_links;
    std::vector<LegacySnapshotDisk> snapshots;

    [[nodiscard]] const std::filesystem::path& target_for(
        const std::filesystem::path& source) const;
};

/**
 * Retained-copy target transaction. Owns the primitives that turn a legacy Hyper-V disk
 * graph into a private, self-contained copy under the dedicated `hyperv_api` target
 * root, without ever mutating the source. All disk mutation happens on the target
 * copies only.
 */
class TargetMigrationTransaction
{
public:
    TargetMigrationTransaction(std::string vm_name, std::filesystem::path target_instance_dir);
    ~TargetMigrationTransaction();

    TargetMigrationTransaction(const TargetMigrationTransaction&) = delete;
    TargetMigrationTransaction& operator=(const TargetMigrationTransaction&) = delete;

    /**
     * Map every unique source disk to a deterministic target-local path under @p root,
     * resolving snapshot parent/child links. Rejects cross-instance/shared backing that
     * is reachable from a graph disk but not part of the graph.
     */
    [[nodiscard]] TargetDiskMapping plan(const LegacyDiskLayout& layout,
                                         const std::filesystem::path& root) const;

    /**
     * Preflight per-instance target space using the source logical file lengths plus a
     * bounded migration/state overhead. Throws when the target volume cannot hold the copy.
     */
    void check_space(const LegacyDiskLayout& layout) const;

    /**
     * Create the uncommitted target directory, copy every unique disk preserving sparseness,
     * reparent the target copies to their target-local parents, copy and rewrite the snapshot
     * bookkeeping to point at the target copies, and persist the transaction manifest.
     */
    [[nodiscard]] TargetDiskMapping stage(const LegacyDiskLayout& layout,
                                          const std::filesystem::path& source_instance_dir);

    /**
     * Verify the copied graph: target file lengths match the source, every VirtDisk parent
     * link reopens against a target-local parent, and every target path is target-local.
     */
    void verify(const TargetDiskMapping& mapping, const LegacyDiskLayout& layout) const;

    /**
     * Mark the verified target as prepared and persist HCS ownership.
     */
    void commit(const TargetDiskMapping& mapping);

    /**
     * Remove the manifest-owned, pre-commit target directory. Committed targets are
     * preserved. Safe to call from noexcept cleanup paths.
     */
    void rollback() noexcept;

private:
    void copy_instance_bookkeeping(const std::filesystem::path& source_instance_dir) const;
    void write_snapshot_bookkeeping(const TargetDiskMapping& mapping,
                                    const std::filesystem::path& source_instance_dir) const;
    [[nodiscard]] MigrationTransactionManifest make_manifest(std::string phase) const;

    std::string vm_name;
    std::string transaction_id;
    std::filesystem::path target_instance_dir;
    bool staged{false};
    bool committed{false};
};

} // namespace multipass::hyperv
