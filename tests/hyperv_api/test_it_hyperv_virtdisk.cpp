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

#include "tests/common.h"

#include <fmt/xchar.h>

#include <filesystem>

#include <src/platform/backends/hyperv_api/virtdisk/virtdisk_api_wrapper.h>
#include <src/platform/backends/hyperv_api/virtdisk/virtdisk_disk_info.h>

namespace multipass::test
{

using uut_t = hyperv::virtdisk::VirtDiskWrapper;

struct HyperVVirtDisk_IntegrationTests : public ::testing::Test
{
};

struct auto_remove_path
{

    auto_remove_path(std::filesystem::path p) : path(p)
    {
    }

    ~auto_remove_path()
    {
        std::error_code ec{};
        std::filesystem::remove(path, ec);
    }

    operator std::filesystem::path() const noexcept
    {
        return path;
    }

private:
    const std::filesystem::path path;
};

auto_remove_path make_tempfile_path(std::string extension)
{
    char pattern[] = "temp-XXXXXX";
    _mktemp_s(pattern);
    std::string f = pattern;
    f.append(extension);
    return {std::filesystem::temp_directory_path() / f};
}

TEST_F(HyperVVirtDisk_IntegrationTests, create_virtual_disk_vhdx)
{
    auto temp_path = make_tempfile_path(".vhdx");
    std::wprintf(L"Path: %s\n", static_cast<std::filesystem::path>(temp_path).c_str());

    uut_t uut{};
    hyperv::virtdisk::CreateVirtualDiskParameters params{};
    params.path = temp_path;
    params.size_in_bytes = 1024 * 1024 * 1024; // 1 GiB

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
    params.size_in_bytes = 1024 * 1024 * 1024; // 1 GiB

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
    params.size_in_bytes = 1024 * 1024 * 1024; // 1 GiB

    const auto c_result = uut.create_virtual_disk(params);
    ASSERT_TRUE(c_result);
    ASSERT_TRUE(c_result.status_msg.empty());

    hyperv::virtdisk::VirtualDiskInfo info{};
    const auto g_result = uut.get_virtual_disk_info(temp_path, info);

    ASSERT_TRUE(info.virtual_storage_type.has_value());
    ASSERT_TRUE(info.size.has_value());

    ASSERT_STREQ(info.virtual_storage_type.value().c_str(), "vhdx");
    ASSERT_EQ(info.size->virtual_, 1024 * 1024 * 1024);
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
    params.size_in_bytes = 1024 * 1024 * 1024; // 1 GiB

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

TEST_F(HyperVVirtDisk_IntegrationTests, DISABLED_resize_shrink)
{
    auto temp_path = make_tempfile_path(".vhdx");
    std::wprintf(L"Path: %s\n", static_cast<std::filesystem::path>(temp_path).c_str());

    uut_t uut{};
    hyperv::virtdisk::CreateVirtualDiskParameters params{};
    params.path = temp_path;
    params.size_in_bytes = 1024 * 1024 * 1024; // 1 GiB

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

    const auto r_result = uut.resize_virtual_disk(temp_path, params.size_in_bytes / 2);
    ASSERT_TRUE(r_result);

    info = {};

    // SmallestSafeVirtualSize

    const auto g2_result = uut.get_virtual_disk_info(temp_path, info);

    ASSERT_TRUE(g2_result);
    ASSERT_TRUE(info.virtual_storage_type.has_value());
    ASSERT_TRUE(info.size.has_value());

    ASSERT_STREQ(info.virtual_storage_type.value().c_str(), "vhdx");
    ASSERT_EQ(info.size->virtual_, params.size_in_bytes / 2);
    ASSERT_EQ(info.size->block, 1024 * 1024);
    ASSERT_EQ(info.size->sector, 512);

    fmt::print("{}", info);
}

} // namespace multipass::test
