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
// Identifies migration-owned directories for rollback and crash recovery.
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

    [[nodiscard]] const std::filesystem::path& target_for(
        const std::filesystem::path& source) const;
};

// Copies and reparents disks without mutating the source. Uncommitted copies are rolled back.
class TargetMigrationTransaction
{
public:
    TargetMigrationTransaction(std::string vm_name, std::filesystem::path target_instance_dir);
    ~TargetMigrationTransaction();

    TargetMigrationTransaction(const TargetMigrationTransaction&) = delete;
    TargetMigrationTransaction& operator=(const TargetMigrationTransaction&) = delete;

    // Reject parents outside the source graph and resolve target filename collisions.
    [[nodiscard]] TargetDiskMapping plan(const LegacyDiskLayout& layout,
                                         const std::filesystem::path& root) const;

    void check_space(const LegacyDiskLayout& layout) const;

    [[nodiscard]] TargetDiskMapping stage(const LegacyDiskLayout& layout,
                                          const std::filesystem::path& source_instance_dir);

    // Check copied lengths and reopen parent links to ensure the graph is target-local.
    void verify(const TargetDiskMapping& mapping) const;

    // Preserve prepared disks until the record store commits or recovers them.
    void commit(const TargetDiskMapping& mapping);
    void rollback() noexcept;

private:
    void write_snapshot_bookkeeping(const LegacyDiskLayout& layout,
                                    const std::filesystem::path& source_instance_dir) const;

    MigrationTransactionManifest manifest;
    std::filesystem::path target_instance_dir;
    bool staged{false};
    bool committed{false};
};

} // namespace multipass::hyperv
