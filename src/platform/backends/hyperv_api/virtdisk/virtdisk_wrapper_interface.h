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

#include <filesystem>
#include <optional>

namespace multipass::hyperv::virtdisk
{

/**
 * Abstract interface for the virtdisk API wrapper.
 */
struct VirtDiskWrapperInterface
{
    virtual OperationResult create_virtual_disk(
        const CreateVirtualDiskParameters& params) const = 0;
    virtual OperationResult resize_virtual_disk(const std::filesystem::path& vhdx_path,
                                                std::uint64_t new_size_bytes) const = 0;
    virtual OperationResult merge_virtual_disk_to_parent(
        const std::filesystem::path& child) const = 0;
    virtual OperationResult reparent_virtual_disk(const std::filesystem::path& child,
                                                  const std::filesystem::path& parent) const = 0;
    virtual OperationResult get_virtual_disk_info(const std::filesystem::path& vhdx_path,
                                                  VirtualDiskInfo& vdinfo) const = 0;

    virtual OperationResult list_virtual_disk_chain(
        const std::filesystem::path& vhdx_path,
        std::vector<std::filesystem::path>& chain,
        std::optional<std::size_t> max_depth = std::nullopt) const = 0;

    virtual ~VirtDiskWrapperInterface() = default;
};
} // namespace multipass::hyperv::virtdisk
