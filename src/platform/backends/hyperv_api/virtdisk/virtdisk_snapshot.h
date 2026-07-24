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

protected:
    void capture_impl() override;
    void erase_impl() override;
    void apply_impl() override;

private:
    [[nodiscard]] static std::string make_snapshot_filename(const Snapshot& ss);
    [[nodiscard]] std::filesystem::path make_snapshot_path(const Snapshot& ss) const;

    /**
     * Create a new differencing child disk.
     *
     * @param [in] parent Existing parent disk.
     * @param [in] child New child disk path.
     */
    void create_new_child_disk(const std::filesystem::path& parent,
                               const std::filesystem::path& child) const;

    void try_rename(const std::filesystem::path& from, const std::filesystem::path& to) noexcept;

    /**
     * Return this snapshot's direct disk children, excluding this snapshot's own file.
     *
     * Includes snapshot files and the live VM disk when they sit directly on @p parent_disk.
     */
    [[nodiscard]] std::vector<std::filesystem::path> get_children_of_disk(
        const std::filesystem::path& parent_disk) const;

    /**
     * Path to the VM's live disk. This path remains stable while the file at it becomes
     * a numbered snapshot and is replaced by a new differencing disk during capture.
     */
    const std::filesystem::path live_disk_path{};

    /**
     * Owning VM.
     */
    const VirtualMachine& vm;
};

} // namespace multipass::hyperv::virtdisk
