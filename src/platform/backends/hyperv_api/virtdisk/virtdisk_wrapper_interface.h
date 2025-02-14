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

#ifndef MULTIPASS_HYPERV_API_VIRTDISK_WRAPPER_INTERFACE_H
#define MULTIPASS_HYPERV_API_VIRTDISK_WRAPPER_INTERFACE_H

#include "../hyperv_api_operation_result.h"
#include "virtdisk_create_virtual_disk_params.h"
#include "virtdisk_disk_info.h"

#include <filesystem>
#include <optional>

namespace multipass::hyperv::virtdisk
{



/**
 *
 *
 */
struct VirtDiskWrapperInterface
{
    virtual OperationResult create_virtual_disk(const CreateVirtualDiskParameters& params) const = 0;
    virtual OperationResult resize_virtual_disk(const std::filesystem::path& vhdx_path,
                                                std::uint64_t new_size_bytes) const = 0;
    virtual OperationResult get_virtual_disk_info(const std::filesystem::path& vhdx_path,
                                                  VirtualDiskInfo& vdinfo) const = 0;
    virtual ~VirtDiskWrapperInterface() = default;
};
} // namespace multipass::hyperv::virtdisk

#endif