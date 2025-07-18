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

#include <hyperv_api/hyperv_api_wrapper_fwdecl.h>
#include <shared/base_snapshot.h>

#include <QString>

#include <filesystem>

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
    /**
     * Create a new Virtdisk snapshot
     *
     * @param [in] name Name for the snapshot
     * @param [in] comment Comment (optional)
     * @param [in] cloud_init_instance_id Name of the VM
     * @param [in] parent Parent, if exists
     * @param [in] specs VM specs
     * @param [in] vm Snapshot owner VM
     * @param [in] desc Owner VM description
     * @param [in] virtdisk Virtdisk API object
     */
    VirtDiskSnapshot(const std::string& name,
                     const std::string& comment,
                     const std::string& cloud_init_instance_id,
                     std::shared_ptr<Snapshot> parent,
                     const VMSpecs& specs,
                     const VirtualMachine& vm,
                     const VirtualMachineDescription& desc,
                     virtdisk_sptr_t virtdisk);

    /**
     * Load an existing VirtDiskSnapshot from file
     *
     * @param [in] filename JSON file
     * @param [in] vm Snapshot owner VM
     * @param [in] desc Owner VM description
     * @param [in] virtdisk Virtdisk API object
     */
    VirtDiskSnapshot(const QString& filename,
                     VirtualMachine& vm,
                     const VirtualMachineDescription& desc,
                     virtdisk_sptr_t virtdisk);

    /**
     * Create a consistent filename for a snapshot
     *
     * @param [in] ss The snapshot
     * @return std::string Filename for the snapshot
     */
    static std::string make_snapshot_filename(const Snapshot& ss);

    /**
     * Retrieve the path for a snapshot
     *
     * @param [in] ss The snapshot
     * @return std::filesystem::path The path that the snapshot is at
     */
    std::filesystem::path make_snapshot_path(const Snapshot& ss) const;

    /**
     * The name for the head disk
     *
     * @return std::string_view Head disk filename
     */
    static constexpr std::string_view head_disk_name() noexcept
    {
        return "head.avhdx";
    }

protected:
    void capture_impl() override;
    void erase_impl() override;
    void apply_impl() override;

private:
    /**
     * Create a new differencing child disk from the parent
     *
     * @param [in] parent Parent of the new differencing child disk. Must already exist.
     * @param [in] child Where to create the child disk. Must be non-existent.
     */
    void create_new_child_disk(const std::filesystem::path& parent,
                               const std::filesystem::path& child) const;

    /**
     * Change the parent disk of the snapshot differencing disks
     *
     * @param [in] snapshots The list of snapshots to reparent the differencing disks of
     * @param [in] new_parent The path to the new parent virtual disk
     */
    void reparent_snapshot_disks(const VirtualMachine::SnapshotVista& snapshots,
                                 const std::filesystem::path& new_parent) const;

    /**
     * Path to the base disk, i.e. the ancestor of all differencing disks.
     */
    const std::filesystem::path base_vhdx_path{};

    /**
     * The owning VM
     */
    const VirtualMachine& vm;

    /**
     * VirtDisk API object
     */
    virtdisk_sptr_t virtdisk{nullptr};
};

} // namespace multipass::hyperv::virtdisk
