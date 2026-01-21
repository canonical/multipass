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

#include <hyperv_api/virtdisk/virtdisk_wrapper.h>

#include "tests/mock_singleton_helpers.h"

namespace multipass::test
{

/**
 * Mock virtdisk API wrapper for testing.
 */
struct MockVirtDiskWrapper : public hyperv::virtdisk::VirtDiskWrapper
{
    using VirtDiskWrapper::VirtDiskWrapper;

    MOCK_METHOD(hyperv::OperationResult,
                create_virtual_disk,
                (const hyperv::virtdisk::CreateVirtualDiskParameters& params),
                (const, override));

    MOCK_METHOD(hyperv::OperationResult,
                resize_virtual_disk,
                (const std::filesystem::path& vhdx_path, std::uint64_t new_size_bytes),
                (const, override));

    MOCK_METHOD(hyperv::OperationResult,
                merge_virtual_disk_to_parent,
                (const std::filesystem::path& child),
                (const, override));

    MOCK_METHOD(hyperv::OperationResult,
                reparent_virtual_disk,
                (const std::filesystem::path& child, const std::filesystem::path& parent),
                (const, override));

    MOCK_METHOD(hyperv::OperationResult,
                get_virtual_disk_info,
                (const std::filesystem::path& vhdx_path, hyperv::virtdisk::VirtualDiskInfo& vdinf),
                (const, override));

    MOCK_METHOD(hyperv::OperationResult,
                list_virtual_disk_chain,
                (const std::filesystem::path& vhdx_path,
                 std::vector<std::filesystem::path>& chain,
                 std::optional<std::size_t> max_depth),
                (const, override));

    MP_MOCK_SINGLETON_BOILERPLATE(MockVirtDiskWrapper, VirtDiskWrapper);
};
} // namespace multipass::test
