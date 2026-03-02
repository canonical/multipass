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

#include <hyperv_api/hyperv_api_operation_result.h>
#include <hyperv_api/virtdisk/virtdisk_create_virtual_disk_params.h>
#include <hyperv_api/virtdisk/virtdisk_disk_info.h>

#include <multipass/singleton.h>

namespace multipass::hyperv::virtdisk
{

/**
 * A high-level wrapper class that defines
 * the common operations that VirtDisk API
 * provide.
 */
struct VirtDiskWrapper : public Singleton<VirtDiskWrapper>
{
    /**
     * Construct a new VirtDiskWrapper
     */
    VirtDiskWrapper(const Singleton<VirtDiskWrapper>::PrivatePass&) noexcept;

    // ---------------------------------------------------------

    /**
     * Create a new Virtual Disk
     *
     * @param [in] params Parameters for the new virtual disk
     */
    [[nodiscard]] virtual OperationResult create_virtual_disk(
        const CreateVirtualDiskParameters& params) const;

    // ---------------------------------------------------------

    /**
     * Resize an existing Virtual Disk
     *
     * @param [in] vhdx_path Path to the virtual disk
     * @param [in] new_size New disk size, in bytes
     */
    [[nodiscard]] virtual OperationResult
    resize_virtual_disk(const std::filesystem::path& vhdx_path, std::uint64_t new_size_bytes) const;

    // ---------------------------------------------------------

    /**
     * Merge a child differencing disk into its parent
     *
     * @param [in] child Path to the differencing disk
     */
    [[nodiscard]] virtual OperationResult merge_virtual_disk_into_parent(
        const std::filesystem::path& child) const;

    // ---------------------------------------------------------

    /**
     * Reparent a virtual disk
     *
     * @param [in] child Path to the virtual disk to reparent
     * @param [in] parent Path to the new parent
     */
    [[nodiscard]] virtual OperationResult reparent_virtual_disk(
        const std::filesystem::path& child,
        const std::filesystem::path& parent) const;

    // ---------------------------------------------------------

    /**
     * Get information about an existing Virtual Disk
     *
     * @param [in] vhdx_path Path to the virtual disk
     * @param [out] vdinfo Virtual disk info output object
     */
    [[nodiscard]] virtual OperationResult
    get_virtual_disk_info(const std::filesystem::path& vhdx_path, VirtualDiskInfo& vdinfo) const;

    /**
     * List all the virtual disks in a virtual disk chain.
     *
     * @param [in] vhdx_path The chain link to start from
     * @param [out] chain The result
     * @param [in] max_depth Maximum depth to list (optional)
     */
    [[nodiscard]] virtual OperationResult list_virtual_disk_chain(
        const std::filesystem::path& vhdx_path,
        std::vector<std::filesystem::path>& chain,
        std::optional<std::size_t> max_depth = std::nullopt) const;
};

inline const VirtDiskWrapper& VirtDisk()
{
    return VirtDiskWrapper::instance();
}

} // namespace multipass::hyperv::virtdisk
