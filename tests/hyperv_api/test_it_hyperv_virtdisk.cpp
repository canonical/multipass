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

#include "hyperv_test_utils.h"
#include "tests/common.h"

#include <fmt/xchar.h>

#include <filesystem>

#include <src/platform/backends/hyperv_api/virtdisk/virtdisk_api_wrapper.h>
#include <src/platform/backends/hyperv_api/virtdisk/virtdisk_disk_info.h>

namespace multipass::test
{

/**
 * 16 MiB
 */
constexpr static auto kTestVhdxSize = 1024 * 1024 * 16ULL;

using uut_t = hyperv::virtdisk::VirtDiskWrapper;

struct HyperVVirtDisk_IntegrationTests : public ::testing::Test
{
};

TEST_F(HyperVVirtDisk_IntegrationTests, create_virtual_disk_vhdx)
{
    auto temp_path = make_tempfile_path(".vhdx");
    std::wprintf(L"Path: %s\n", static_cast<std::filesystem::path>(temp_path).c_str());

    uut_t uut{};
    hyperv::virtdisk::CreateVirtualDiskParameters params{};
    params.path = temp_path;
    params.size_in_bytes = kTestVhdxSize;

    const auto result = uut.create_virtual_disk(params);
    ASSERT_TRUE(result);
    ASSERT_TRUE(result.status_msg.empty());
}

TEST_F(HyperVVirtDisk_IntegrationTests, create_virtual_disk_vhd)
{
    auto temp_path = make_tempfile_path(".vhd");
    std::wprintf(L"Path: %s\n", static_cast<std::filesystem::path>(temp_path).c_str());

    uut_t uut{};
    hyperv::virtdisk::CreateVirtualDiskParameters params{};
    params.path = temp_path;
    params.size_in_bytes = kTestVhdxSize;

    const auto result = uut.create_virtual_disk(params);
    ASSERT_TRUE(result);
    ASSERT_TRUE(result.status_msg.empty());
}

TEST_F(HyperVVirtDisk_IntegrationTests, get_virtual_disk_properties)
{
    auto temp_path = make_tempfile_path(".vhdx");
    std::wprintf(L"Path: %s\n", static_cast<std::filesystem::path>(temp_path).c_str());

    uut_t uut{};
    hyperv::virtdisk::CreateVirtualDiskParameters params{};
    params.path = temp_path;
    params.size_in_bytes = kTestVhdxSize;

    const auto c_result = uut.create_virtual_disk(params);
    ASSERT_TRUE(c_result);
    ASSERT_TRUE(c_result.status_msg.empty());

    hyperv::virtdisk::VirtualDiskInfo info{};
    const auto g_result = uut.get_virtual_disk_info(temp_path, info);

    ASSERT_TRUE(info.virtual_storage_type.has_value());
    ASSERT_TRUE(info.size.has_value());

    ASSERT_STREQ(info.virtual_storage_type.value().c_str(), "vhdx");
    ASSERT_EQ(info.size->virtual_, params.size_in_bytes);
    ASSERT_EQ(info.size->block, 1024 * 1024);
    ASSERT_EQ(info.size->sector, 512);

    fmt::print("{}", info);
}

TEST_F(HyperVVirtDisk_IntegrationTests, resize_grow)
{
    auto temp_path = make_tempfile_path(".vhdx");
    std::wprintf(L"Path: %s\n", static_cast<std::filesystem::path>(temp_path).c_str());

    uut_t uut{};
    hyperv::virtdisk::CreateVirtualDiskParameters params{};
    params.path = temp_path;
    params.size_in_bytes = kTestVhdxSize;

    const auto c_result = uut.create_virtual_disk(params);
    ASSERT_TRUE(c_result);
    ASSERT_TRUE(c_result.status_msg.empty());

    hyperv::virtdisk::VirtualDiskInfo info{};
    const auto g_result = uut.get_virtual_disk_info(temp_path, info);

    ASSERT_TRUE(g_result);
    ASSERT_TRUE(info.virtual_storage_type.has_value());
    ASSERT_TRUE(info.size.has_value());

    ASSERT_STREQ(info.virtual_storage_type.value().c_str(), "vhdx");
    ASSERT_EQ(info.size->virtual_, params.size_in_bytes);
    ASSERT_EQ(info.size->block, 1024 * 1024);
    ASSERT_EQ(info.size->sector, 512);

    fmt::print("{}", info);

    const auto r_result = uut.resize_virtual_disk(temp_path, params.size_in_bytes * 2);
    ASSERT_TRUE(r_result);

    info = {};

    const auto g2_result = uut.get_virtual_disk_info(temp_path, info);

    ASSERT_TRUE(g2_result);
    ASSERT_TRUE(info.virtual_storage_type.has_value());
    ASSERT_TRUE(info.size.has_value());

    ASSERT_STREQ(info.virtual_storage_type.value().c_str(), "vhdx");
    ASSERT_EQ(info.size->virtual_, params.size_in_bytes * 2);
    ASSERT_EQ(info.size->block, 1024 * 1024);
    ASSERT_EQ(info.size->sector, 512);

    fmt::print("{}", info);
}

TEST_F(HyperVVirtDisk_IntegrationTests, create_child_disk)
{
    uut_t uut{};
    // Create parent
    auto parent_temp_path = make_tempfile_path(".vhdx");
    std::wprintf(L"Parent Path: %s\n",
                 static_cast<std::filesystem::path>(parent_temp_path).c_str());
    {
        hyperv::virtdisk::CreateVirtualDiskParameters params{};
        params.path = parent_temp_path;
        params.size_in_bytes = kTestVhdxSize;

        const auto result = uut.create_virtual_disk(params);
        ASSERT_TRUE(result);
        ASSERT_TRUE(result.status_msg.empty());
    }
    // Create child
    auto child_temp_path = make_tempfile_path(".avhdx");
    std::wprintf(L"Child Path: %s\n", static_cast<std::filesystem::path>(child_temp_path).c_str());
    {
        hyperv::virtdisk::CreateVirtualDiskParameters params{};
        params.predecessor = hyperv::virtdisk::ParentPathParameters{parent_temp_path};
        params.path = child_temp_path;

        const auto result = uut.create_virtual_disk(params);
        ASSERT_TRUE(result);
        ASSERT_TRUE(result.status_msg.empty());
    }
}

TEST_F(HyperVVirtDisk_IntegrationTests, merge_virtual_disk)
{
    uut_t uut{};
    // Create parent
    auto parent_temp_path = make_tempfile_path(".vhdx");
    std::wprintf(L"Parent Path: %s\n",
                 static_cast<std::filesystem::path>(parent_temp_path).c_str());
    {
        hyperv::virtdisk::CreateVirtualDiskParameters params{};
        params.path = parent_temp_path;
        params.size_in_bytes = kTestVhdxSize;

        const auto result = uut.create_virtual_disk(params);
        ASSERT_TRUE(result);
        ASSERT_TRUE(result.status_msg.empty());
    }
    // Create child
    auto child_temp_path = make_tempfile_path(".avhdx");
    std::wprintf(L"Child Path: %s\n", static_cast<std::filesystem::path>(child_temp_path).c_str());
    {
        hyperv::virtdisk::CreateVirtualDiskParameters params{};
        params.predecessor = hyperv::virtdisk::ParentPathParameters{parent_temp_path};
        params.path = child_temp_path;

        const auto result = uut.create_virtual_disk(params);
        ASSERT_TRUE(result);
        ASSERT_TRUE(result.status_msg.empty());
    }

    // Merge child to parent
    const auto result = uut.merge_virtual_disk_to_parent(child_temp_path);
    ASSERT_TRUE(result);
    ASSERT_TRUE(result.status_msg.empty());
}

TEST_F(HyperVVirtDisk_IntegrationTests, merge_reparent_virtual_disk)
{
    uut_t uut{};
    // Create parent
    auto parent_temp_path = make_tempfile_path(".vhdx");
    std::wprintf(L"Parent Path: %s\n",
                 static_cast<std::filesystem::path>(parent_temp_path).c_str());
    {
        hyperv::virtdisk::CreateVirtualDiskParameters params{};
        params.path = parent_temp_path;
        params.size_in_bytes = kTestVhdxSize;

        const auto result = uut.create_virtual_disk(params);
        ASSERT_TRUE(result);
        ASSERT_TRUE(result.status_msg.empty());
    }
    // Create child
    auto child_temp_path = make_tempfile_path(".avhdx");
    std::wprintf(L"Child Path: %s\n", static_cast<std::filesystem::path>(child_temp_path).c_str());
    {
        hyperv::virtdisk::CreateVirtualDiskParameters params{};
        params.predecessor = hyperv::virtdisk::ParentPathParameters{parent_temp_path};
        params.path = child_temp_path;

        const auto result = uut.create_virtual_disk(params);
        ASSERT_TRUE(result);
        ASSERT_TRUE(result.status_msg.empty());
    }

    // Create grandchild
    auto grandchild_temp_path = make_tempfile_path(".avhdx");
    std::wprintf(L"Grandchild Path: %s\n",
                 static_cast<std::filesystem::path>(grandchild_temp_path).c_str());
    {
        hyperv::virtdisk::CreateVirtualDiskParameters params{};
        params.predecessor = hyperv::virtdisk::ParentPathParameters{child_temp_path};
        params.path = grandchild_temp_path;

        const auto result = uut.create_virtual_disk(params);
        ASSERT_TRUE(result);
        ASSERT_TRUE(result.status_msg.empty());
    }

    // Merge child to parent
    {
        const auto result = uut.merge_virtual_disk_to_parent(child_temp_path);
        ASSERT_TRUE(result);
        ASSERT_TRUE(result.status_msg.empty());
    }

    // Reparent grandchild to parent
    {
        const auto result = uut.reparent_virtual_disk(grandchild_temp_path, parent_temp_path);
        ASSERT_TRUE(result);
        ASSERT_TRUE(result.status_msg.empty());
    }
}

TEST_F(HyperVVirtDisk_IntegrationTests, DISABLED_list_parents)
{
    uut_t uut{};
    // Create parent
    auto parent_temp_path = make_tempfile_path(".vhdx");
    std::wprintf(L"Parent Path: %s\n",
                 static_cast<std::filesystem::path>(parent_temp_path).c_str());
    {
        hyperv::virtdisk::CreateVirtualDiskParameters params{};
        params.path = parent_temp_path;
        params.size_in_bytes = kTestVhdxSize;

        const auto result = uut.create_virtual_disk(params);
        ASSERT_TRUE(result);
        ASSERT_TRUE(result.status_msg.empty());
    }
    // Create child #1
    auto child1_temp_path = make_tempfile_path(".avhdx");

    std::wprintf(L"Child Path: %s\n", static_cast<std::filesystem::path>(child1_temp_path).c_str());
    {
        hyperv::virtdisk::CreateVirtualDiskParameters params{};
        params.predecessor = hyperv::virtdisk::ParentPathParameters{parent_temp_path};
        params.path = child1_temp_path;

        const auto result = uut.create_virtual_disk(params);
        ASSERT_TRUE(result);
        ASSERT_TRUE(result.status_msg.empty());
    }

    // Create child #2
    auto child2_temp_path = make_tempfile_path(".avhdx");
    std::wprintf(L"Child Path: %s\n", static_cast<std::filesystem::path>(child2_temp_path).c_str());
    {
        hyperv::virtdisk::CreateVirtualDiskParameters params{};
        params.predecessor = hyperv::virtdisk::ParentPathParameters{child1_temp_path};
        params.path = child2_temp_path;

        const auto result = uut.create_virtual_disk(params);
        ASSERT_TRUE(result);
        ASSERT_TRUE(result.status_msg.empty());
    }

    // Try to list

    std::vector<std::filesystem::path> result{};
    ASSERT_TRUE(uut.list_virtual_disk_chain(child2_temp_path, result));
    ASSERT_EQ(result.size(), 3);

    EXPECT_TRUE(std::filesystem::equivalent(result[0], child2_temp_path));
    EXPECT_TRUE(std::filesystem::equivalent(result[1], child1_temp_path));
    EXPECT_TRUE(std::filesystem::equivalent(result[2], parent_temp_path));
    EXPECT_EQ(result[0], child2_temp_path);
    EXPECT_EQ(result[1], child1_temp_path);
    EXPECT_EQ(result[2], parent_temp_path);
    for (const auto& path : result)
    {
        std::wprintf(L"Child Path: %s\n", path.c_str());
    }
}

} // namespace multipass::test
