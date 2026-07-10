/*
 * Copyright (C) Canonical, Ltd.
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

#pragma once

#include <shared/base_snapshot.h>

#include <filesystem>
#include <string_view>
#include <vector>

namespace multipass::hyperv::virtdisk
{

/**
 * Virtdisk-based snapshot implementation.
 *
 * The implementation uses differencing disks to realize the
 * functionality.
 */
class VirtDiskSnapshot : public BaseSnapshot
{
public:
    VirtDiskSnapshot(const std::string& name,
                     const std::string& comment,
                     const std::string& cloud_init_instance_id,
                     std::shared_ptr<Snapshot> parent,
                     const VMSpecs& specs,
                     const VirtualMachine& vm,
                     const VirtualMachineDescription& desc);
    VirtDiskSnapshot(const std::filesystem::path& filename,
                     VirtualMachine& vm,
                     const VirtualMachineDescription& desc);

    [[nodiscard]] static constexpr std::string_view head_disk_name() noexcept
    {
        return "head.avhdx";
    }

    /**
     * Fold a lingering differencing "head" disk back into its base disk and remove
     * it, so the VM runs on a standalone base again.
     *
     * A no-op unless a head disk exists and is a *direct* child of the base (which
     * holds exactly when no snapshots remain). This restores the pre-snapshot disk
     * layout after the last snapshot is deleted and, in particular, re-enables disk
     * resize (a base disk cannot be resized while a differencing child sits on it).
     *
     * Safe without copying the base: MergeVirtualDisk only reads the head and writes
     * into the base, and the head is removed only *after* a fully successful merge, so
     * a failed or interrupted merge leaves the `base <- head` chain intact and readable
     * (the head still overrides every block). Throws on merge failure; callers that
     * treat the collapse as an optimization should catch and continue.
     *
     * @param [in] base_vhdx_path Path to the base disk of the differencing chain.
     */
    static void collapse_head_into_base(const std::filesystem::path& base_vhdx_path);

protected:
    void capture_impl() override;
    void erase_impl() override;
    void apply_impl() override;

private:
    /**
     * When the snapshot just erased was the last one, fold the now-direct head disk back
     * into the base (via collapse_head_into_base) so the VM runs on a standalone disk
     * again. Best-effort: a collapse failure is logged, not propagated, since the erase
     * itself has already succeeded.
     */
    void collapse_head_after_last_erase();

    [[nodiscard]] static std::string make_snapshot_filename(const Snapshot& ss);
    [[nodiscard]] std::filesystem::path make_snapshot_path(const Snapshot& ss) const;
    [[nodiscard]] std::filesystem::path make_head_disk_path() const;

    /**
     * Create a new differencing child disk from the parent
     *
     * @param [in] parent Parent of the new differencing child disk. Must already exist.
     * @param [in] child Where to create the child disk. Must be non-existent.
     */
    void create_new_child_disk(const std::filesystem::path& parent,
                               const std::filesystem::path& child) const;

    /**
     * All differencing disks that sit *directly* on @p parent_disk: the snapshot files
     * whose immediate parent is @p parent_disk, plus the live head disk if it is attached
     * there. This snapshot's own file is never included.
     *
     * @param [in] parent_disk Disk whose direct children to enumerate.
     */
    [[nodiscard]] std::vector<std::filesystem::path> get_disk_children(
        const std::filesystem::path& parent_disk) const;

    /**
     * Path to the base disk, i.e. the ancestor of all differencing disks.
     */
    const std::filesystem::path base_vhdx_path{};

    /**
     * The owning VM
     */
    const VirtualMachine& vm;
};

} // namespace multipass::hyperv::virtdisk
