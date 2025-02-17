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

#ifndef MULTIPASS_HYPERV_API_HCS_WRAPPER
#define MULTIPASS_HYPERV_API_HCS_WRAPPER

#include "virtdisk_api_table.h"
#include "virtdisk_disk_info.h"
#include "virtdisk_wrapper_interface.h"

namespace multipass::hyperv::virtdisk
{

/**
 * A high-level wrapper class that defines
 * the common operations that Host Compute System
 * API provide.
 */
struct VirtDiskWrapper : public VirtDiskWrapperInterface
{

    /**
     * Construct a new HCNWrapper
     *
     * @param api_table The HCN API table object (optional)
     *
     * The wrapper will use the real HCN API by default.
     */
    VirtDiskWrapper(const VirtDiskAPITable& api_table = {});
    VirtDiskWrapper(const VirtDiskWrapper&) = default;
    VirtDiskWrapper(VirtDiskWrapper&&) = default;
    VirtDiskWrapper& operator=(const VirtDiskWrapper&) = delete;
    VirtDiskWrapper& operator=(VirtDiskWrapper&&) = delete;

    // ---------------------------------------------------------

    /**
     * Create a new Virtual Disk
     *
     * @param [in] params Parameters for the new virtual disk
     *
     * @return An object that evaluates to true on success, false otherwise.
     * message() may contain details of failure when result is false.
     */
    [[nodiscard]] OperationResult create_virtual_disk(const CreateVirtualDiskParameters& params) const override;

    // ---------------------------------------------------------

    /**
     * Resize an existing Virtual Disk
     *
     * @param [in] vhdx_path Path to the virtual disk
     * @param [in] new_size New disk size, in bytes
     *
     * @return An object that evaluates to true on success, false otherwise.
     * message() may contain details of failure when result is false.
     */
    virtual OperationResult resize_virtual_disk(const std::filesystem::path& vhdx_path,
                                                std::uint64_t new_size_bytes) const override;

    // ---------------------------------------------------------

    /**
     * Get information about an existing Virtual Disk
     *
     * @param [in] vhdx_path Path to the virtual disk
     * @param [out] vdinfo Virtual disk info output object
     *
     * @return An object that evaluates to true on success, false otherwise.
     * message() may contain details of failure when result is false.
     */
    virtual OperationResult get_virtual_disk_info(const std::filesystem::path& vhdx_path,
                                                  VirtualDiskInfo& vdinfo) const override;

private:
    const VirtDiskAPITable api;
};

} // namespace multipass::hyperv::virtdisk

#endif
