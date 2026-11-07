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

#include "tests/unit/hcs/mock_virtdisk_api.h"
#include "tests/unit/mock_file_ops.h"
#include "tests/unit/mock_logger.h"

#include <hcs/api/virtdisk/virtdisk_disk_info.h>
#include <hcs/api/virtdisk/virtdisk_wrapper.h>

#include <fmt/xchar.h>

#include <windows.h>
#include <winioctl.h>

namespace mpt = multipass::test;
namespace mpl = multipass::logging;

using namespace testing;

namespace multipass::test
{

using hyperv::virtdisk::VirtDisk;

struct HyperVVirtDisk_UnitTests : public ::testing::Test
{
    mpt::MockLogger::Scope logger_scope = [] {
        auto v = mpt::MockLogger::inject();
        v.mock_logger->screen_logs(mpl::Level::info);
        return v;
    }();

    mpt::MockVirtDiskAPI::GuardedMock mock_virtdisk_api_injection =
        mpt::MockVirtDiskAPI::inject<StrictMock>();
    mpt::MockVirtDiskAPI& mock_virtdisk_api = *mock_virtdisk_api_injection.first;

    // Sentinel values as mock API parameters. These handles are opaque handles and
    // they're not being dereferenced in any way -- only address values are compared.
    inline static auto mock_handle_object = reinterpret_cast<HANDLE>(0xbadf00d);

    void open_vhd_expect_failure()
    {
        /******************************************************
         * Verify that the dependencies are called with right
         * data.
         ******************************************************/

        EXPECT_CALL(mock_virtdisk_api, OpenVirtualDisk).WillOnce(Return(ERROR_PATH_NOT_FOUND));
        logger_scope.mock_logger->expect_log(
            mpl::Level::error,
            "open_virtual_disk(...) > OpenVirtualDisk failed with:");
    }
};

// ---------------------------------------------------------

TEST_F(HyperVVirtDisk_UnitTests, create_virtual_disk_vhdx_happy_path)
{
    { // Verify that the dependencies are called with right data.
        EXPECT_CALL(mock_virtdisk_api, CreateVirtualDisk)
            .WillOnce(DoAll(
                [](PVIRTUAL_STORAGE_TYPE VirtualStorageType,
                   PCWSTR Path,
                   VIRTUAL_DISK_ACCESS_MASK VirtualDiskAccessMask,
                   PSECURITY_DESCRIPTOR SecurityDescriptor,
                   CREATE_VIRTUAL_DISK_FLAG Flags,
                   ULONG ProviderSpecificFlags,
                   PCREATE_VIRTUAL_DISK_PARAMETERS Parameters,
                   LPOVERLAPPED Overlapped,
                   PHANDLE Handle) {
                    ASSERT_NE(nullptr, VirtualStorageType);
                    ASSERT_EQ(VirtualStorageType->DeviceId, VIRTUAL_STORAGE_TYPE_DEVICE_VHDX);
                    ASSERT_EQ(VirtualStorageType->VendorId, VIRTUAL_STORAGE_TYPE_VENDOR_UNKNOWN);
                    ASSERT_NE(nullptr, Path);
                    ASSERT_STREQ(Path, L"test.vhdx");

                    ASSERT_EQ(VirtualDiskAccessMask, VIRTUAL_DISK_ACCESS_NONE);
                    ASSERT_EQ(nullptr, SecurityDescriptor);
                    ASSERT_EQ(CREATE_VIRTUAL_DISK_FLAG_NONE, Flags);
                    ASSERT_EQ(0, ProviderSpecificFlags);
                    ASSERT_NE(nullptr, Parameters);
                    ASSERT_EQ(Parameters->Version, CREATE_VIRTUAL_DISK_VERSION_2);
                    ASSERT_EQ(Parameters->Version2.MaximumSize, 2097152);
                    ASSERT_EQ(Parameters->Version2.BlockSizeInBytes, 1048576);

                    ASSERT_EQ(nullptr, Overlapped);
                    ASSERT_NE(nullptr, Handle);
                    ASSERT_EQ(nullptr, *Handle);

                    *Handle = mock_handle_object;
                },
                Return(ERROR_SUCCESS)));

        EXPECT_CALL(mock_virtdisk_api, CloseHandle(Eq(mock_handle_object))).WillOnce(Return(true));
    }

    const hyperv::virtdisk::CreateVirtualDiskParameters params{.size_in_bytes = 2097152,
                                                               .path = "test.vhdx"};

    {
        const auto& [status, status_msg] = VirtDisk().create_virtual_disk(params);
        EXPECT_TRUE(status.success());
        EXPECT_TRUE(status_msg.empty());
    }
}

// ---------------------------------------------------------

TEST_F(HyperVVirtDisk_UnitTests, create_virtual_disk_vhd_happy_path)
{
    { // Verify that the dependencies are called with right data.
        EXPECT_CALL(mock_virtdisk_api, CreateVirtualDisk)
            .WillOnce(DoAll(
                [](PVIRTUAL_STORAGE_TYPE VirtualStorageType,
                   PCWSTR Path,
                   VIRTUAL_DISK_ACCESS_MASK VirtualDiskAccessMask,
                   PSECURITY_DESCRIPTOR SecurityDescriptor,
                   CREATE_VIRTUAL_DISK_FLAG Flags,
                   ULONG ProviderSpecificFlags,
                   PCREATE_VIRTUAL_DISK_PARAMETERS Parameters,
                   LPOVERLAPPED Overlapped,
                   PHANDLE Handle

                ) {
                    ASSERT_NE(nullptr, VirtualStorageType);
                    ASSERT_EQ(VirtualStorageType->DeviceId, VIRTUAL_STORAGE_TYPE_DEVICE_VHDX);
                    ASSERT_EQ(VirtualStorageType->VendorId, VIRTUAL_STORAGE_TYPE_VENDOR_UNKNOWN);
                    ASSERT_NE(nullptr, Path);
                    ASSERT_STREQ(Path, L"test.vhd");
                    ASSERT_EQ(VirtualDiskAccessMask, VIRTUAL_DISK_ACCESS_NONE);
                    ASSERT_EQ(nullptr, SecurityDescriptor);
                    ASSERT_EQ(CREATE_VIRTUAL_DISK_FLAG_NONE, Flags);
                    ASSERT_EQ(0, ProviderSpecificFlags);
                    ASSERT_NE(nullptr, Parameters);
                    ASSERT_EQ(Parameters->Version, CREATE_VIRTUAL_DISK_VERSION_2);
                    ASSERT_EQ(Parameters->Version2.MaximumSize, 2097152);
                    ASSERT_EQ(Parameters->Version2.BlockSizeInBytes, 524288);
                    ASSERT_EQ(nullptr, Overlapped);
                    ASSERT_NE(nullptr, Handle);
                    ASSERT_EQ(nullptr, *Handle);

                    *Handle = mock_handle_object;
                },
                Return(ERROR_SUCCESS)));

        EXPECT_CALL(mock_virtdisk_api, CloseHandle(Eq(mock_handle_object))).WillOnce(Return(true));
    }

    const hyperv::virtdisk::CreateVirtualDiskParameters params{.size_in_bytes = 2097152,
                                                               .path = "test.vhd"};

    {
        const auto& [status, status_msg] = VirtDisk().create_virtual_disk(params);
        EXPECT_TRUE(status.success());
        EXPECT_TRUE(status_msg.empty());
    }
}

// ---------------------------------------------------------

TEST_F(HyperVVirtDisk_UnitTests, create_virtual_disk_vhdx_with_source)
{
    auto [mock_file_ops, guard] = mpt::MockFileOps::inject();
    EXPECT_CALL(*mock_file_ops, exists(TypedEq<const fs::path&>(L"source.vhdx")))
        .WillOnce(Return(true));

    { // Verify that the dependencies are called with right data.
        EXPECT_CALL(mock_virtdisk_api, CreateVirtualDisk)
            .WillOnce(DoAll(
                [](PVIRTUAL_STORAGE_TYPE VirtualStorageType,
                   PCWSTR Path,
                   VIRTUAL_DISK_ACCESS_MASK VirtualDiskAccessMask,
                   PSECURITY_DESCRIPTOR SecurityDescriptor,
                   CREATE_VIRTUAL_DISK_FLAG Flags,
                   ULONG ProviderSpecificFlags,
                   PCREATE_VIRTUAL_DISK_PARAMETERS Parameters,
                   LPOVERLAPPED Overlapped,
                   PHANDLE Handle) {
                    ASSERT_NE(nullptr, VirtualStorageType);
                    ASSERT_EQ(VirtualStorageType->DeviceId, VIRTUAL_STORAGE_TYPE_DEVICE_VHDX);
                    ASSERT_EQ(VirtualStorageType->VendorId, VIRTUAL_STORAGE_TYPE_VENDOR_UNKNOWN);
                    ASSERT_NE(nullptr, Path);
                    ASSERT_STREQ(Path, L"test.vhdx");

                    ASSERT_EQ(VirtualDiskAccessMask, VIRTUAL_DISK_ACCESS_NONE);
                    ASSERT_EQ(nullptr, SecurityDescriptor);
                    ASSERT_EQ(CREATE_VIRTUAL_DISK_FLAG_PREVENT_WRITES_TO_SOURCE_DISK, Flags);
                    ASSERT_EQ(0, ProviderSpecificFlags);
                    ASSERT_NE(nullptr, Parameters);
                    ASSERT_EQ(Parameters->Version, CREATE_VIRTUAL_DISK_VERSION_2);
                    ASSERT_EQ(Parameters->Version2.MaximumSize, 0);
                    ASSERT_EQ(Parameters->Version2.BlockSizeInBytes,
                              CREATE_VIRTUAL_DISK_PARAMETERS_DEFAULT_BLOCK_SIZE);
                    ASSERT_STREQ(Parameters->Version2.SourcePath, L"source.vhdx");
                    ASSERT_EQ(Parameters->Version2.SourceVirtualStorageType.DeviceId,
                              VIRTUAL_STORAGE_TYPE_DEVICE_VHDX);
                    ASSERT_EQ(Parameters->Version2.SourceVirtualStorageType.VendorId,
                              VIRTUAL_STORAGE_TYPE_VENDOR_UNKNOWN);

                    ASSERT_EQ(nullptr, Overlapped);
                    ASSERT_NE(nullptr, Handle);
                    ASSERT_EQ(nullptr, *Handle);

                    *Handle = mock_handle_object;
                },
                Return(ERROR_SUCCESS)));

        EXPECT_CALL(mock_virtdisk_api, OpenVirtualDisk)
            .WillOnce(DoAll(
                [](PVIRTUAL_STORAGE_TYPE VirtualStorageType,
                   PCWSTR Path,
                   VIRTUAL_DISK_ACCESS_MASK VirtualDiskAccessMask,
                   OPEN_VIRTUAL_DISK_FLAG Flags,
                   POPEN_VIRTUAL_DISK_PARAMETERS Parameters,
                   PHANDLE Handle) {
                    ASSERT_NE(nullptr, VirtualStorageType);
                    ASSERT_EQ(VirtualStorageType->DeviceId, VIRTUAL_STORAGE_TYPE_DEVICE_UNKNOWN);
                    ASSERT_EQ(VirtualStorageType->VendorId, VIRTUAL_STORAGE_TYPE_VENDOR_UNKNOWN);
                    ASSERT_NE(nullptr, Path);
                    ASSERT_STREQ(Path, L"source.vhdx");
                    ASSERT_EQ(VIRTUAL_DISK_ACCESS_ALL, VirtualDiskAccessMask);
                    ASSERT_EQ(OPEN_VIRTUAL_DISK_FLAG_NONE, Flags);
                    ASSERT_EQ(nullptr, Parameters);
                    ASSERT_NE(nullptr, Handle);
                    ASSERT_EQ(nullptr, *Handle);

                    *Handle = mock_handle_object;
                },
                Return(ERROR_SUCCESS)));

        // The API will be called for several times.
        EXPECT_CALL(mock_virtdisk_api, GetVirtualDiskInformation)
            .WillRepeatedly(DoAll(
                [](HANDLE VirtualDiskHandle,
                   PULONG VirtualDiskInfoSize,
                   PGET_VIRTUAL_DISK_INFO VirtualDiskInfo,
                   PULONG SizeUsed) {
                    ASSERT_EQ(mock_handle_object, VirtualDiskHandle);
                    ASSERT_NE(nullptr, VirtualDiskInfoSize);
                    ASSERT_EQ(sizeof(GET_VIRTUAL_DISK_INFO), *VirtualDiskInfoSize);
                    ASSERT_NE(nullptr, VirtualDiskInfo);
                    ASSERT_EQ(nullptr, SizeUsed);
                    VirtualDiskInfo->VirtualStorageType.DeviceId = VIRTUAL_STORAGE_TYPE_DEVICE_VHDX;
                    VirtualDiskInfo->VirtualStorageType.VendorId =
                        VIRTUAL_STORAGE_TYPE_VENDOR_UNKNOWN;
                    VirtualDiskInfo->SmallestSafeVirtualSize = 123456;
                    VirtualDiskInfo->Size.VirtualSize = 1111111;
                    VirtualDiskInfo->Size.BlockSize = 2222222;
                    VirtualDiskInfo->Size.PhysicalSize = 3333333;
                    VirtualDiskInfo->Size.SectorSize = 4444444;
                    VirtualDiskInfo->ProviderSubtype = 3; // dynamic
                },
                Return(ERROR_SUCCESS)));

        EXPECT_CALL(mock_virtdisk_api, CloseHandle(Eq(mock_handle_object)))
            .Times(2)
            .WillRepeatedly(Return(true));
    }

    const hyperv::virtdisk::CreateVirtualDiskParameters params{
        .size_in_bytes = 0,
        .path = "test.vhdx",
        .predecessor = hyperv::virtdisk::SourcePathParameters{"source.vhdx"}};

    {
        const auto& [status, status_msg] = VirtDisk().create_virtual_disk(params);
        EXPECT_TRUE(status.success());
        EXPECT_TRUE(status_msg.empty());
    }
}

// ---------------------------------------------------------

TEST_F(HyperVVirtDisk_UnitTests, create_virtual_disk_failed)
{
    { // Verify that the dependencies are called with right data.
        EXPECT_CALL(mock_virtdisk_api, CreateVirtualDisk)
            .WillOnce(DoAll([](PVIRTUAL_STORAGE_TYPE VirtualStorageType,
                               PCWSTR Path,
                               VIRTUAL_DISK_ACCESS_MASK VirtualDiskAccessMask,
                               PSECURITY_DESCRIPTOR SecurityDescriptor,
                               CREATE_VIRTUAL_DISK_FLAG Flags,
                               ULONG ProviderSpecificFlags,
                               PCREATE_VIRTUAL_DISK_PARAMETERS Parameters,
                               LPOVERLAPPED Overlapped,
                               PHANDLE Handle

                            ) {},
                            Return(ERROR_PATH_NOT_FOUND)));
        logger_scope.mock_logger->expect_log(
            mpl::Level::error,
            "create_virtual_disk(...) > CreateVirtualDisk failed with 3!");
    }

    const hyperv::virtdisk::CreateVirtualDiskParameters params{.size_in_bytes = 2097152,
                                                               .path = "test.vhd"};

    {
        const auto& [status, status_msg] = VirtDisk().create_virtual_disk(params);
        EXPECT_FALSE(status.success());
        ASSERT_FALSE(status_msg.empty());
        EXPECT_NE(status_msg.find(L"CreateVirtualDisk failed"), std::wstring::npos);
        EXPECT_NE(status_msg.find(L"(0x80070003)"), std::wstring::npos);
    }
}

// ---------------------------------------------------------

TEST_F(HyperVVirtDisk_UnitTests, resize_virtual_disk_happy_path)
{
    { // Verify that the dependencies are called with right data.
        EXPECT_CALL(mock_virtdisk_api, OpenVirtualDisk)
            .WillOnce(DoAll(
                [](PVIRTUAL_STORAGE_TYPE VirtualStorageType,
                   PCWSTR Path,
                   VIRTUAL_DISK_ACCESS_MASK VirtualDiskAccessMask,
                   OPEN_VIRTUAL_DISK_FLAG Flags,
                   POPEN_VIRTUAL_DISK_PARAMETERS Parameters,
                   PHANDLE Handle) {
                    ASSERT_NE(nullptr, VirtualStorageType);
                    ASSERT_EQ(VirtualStorageType->DeviceId, VIRTUAL_STORAGE_TYPE_DEVICE_UNKNOWN);
                    ASSERT_EQ(VirtualStorageType->VendorId, VIRTUAL_STORAGE_TYPE_VENDOR_UNKNOWN);
                    ASSERT_NE(nullptr, Path);
                    ASSERT_STREQ(Path, L"test.vhdx");
                    ASSERT_EQ(VIRTUAL_DISK_ACCESS_ALL, VirtualDiskAccessMask);
                    ASSERT_EQ(OPEN_VIRTUAL_DISK_FLAG_NONE, Flags);
                    ASSERT_EQ(nullptr, Parameters);
                    ASSERT_NE(nullptr, Handle);
                    ASSERT_EQ(nullptr, *Handle);
                    *Handle = mock_handle_object;
                },
                Return(ERROR_SUCCESS)));

        EXPECT_CALL(mock_virtdisk_api, ResizeVirtualDisk)
            .WillOnce(DoAll(
                [](HANDLE VirtualDiskHandle,
                   RESIZE_VIRTUAL_DISK_FLAG Flags,
                   PRESIZE_VIRTUAL_DISK_PARAMETERS Parameters,
                   LPOVERLAPPED Overlapped) {
                    ASSERT_EQ(mock_handle_object, VirtualDiskHandle);
                    ASSERT_EQ(RESIZE_VIRTUAL_DISK_FLAG_NONE, Flags);
                    ASSERT_NE(nullptr, Parameters);
                    ASSERT_EQ(Parameters->Version, RESIZE_VIRTUAL_DISK_VERSION_1);
                    ASSERT_EQ(Parameters->Version1.NewSize, 1234567);
                    ASSERT_EQ(nullptr, Overlapped);
                },
                Return(ERROR_SUCCESS)));

        EXPECT_CALL(mock_virtdisk_api, CloseHandle(Eq(mock_handle_object))).WillOnce(Return(true));
    }

    {
        const auto& [status, status_msg] = VirtDisk().resize_virtual_disk("test.vhdx", 1234567);
        EXPECT_TRUE(status.success());
        EXPECT_TRUE(status_msg.empty());
    }
}

// ---------------------------------------------------------

TEST_F(HyperVVirtDisk_UnitTests, resize_virtual_disk_open_failed)
{
    { // Verify that the dependencies are called with right data.
        open_vhd_expect_failure();
    }

    {
        const auto& [status, status_msg] = VirtDisk().resize_virtual_disk("test.vhdx", 1234567);
        EXPECT_FALSE(status.success());
        ASSERT_FALSE(status_msg.empty());
        ASSERT_STREQ(status_msg.c_str(), L"open_virtual_disk failed!");
    }
}

// ---------------------------------------------------------

TEST_F(HyperVVirtDisk_UnitTests, resize_virtual_disk_resize_failed)
{

    { // Verify that the dependencies are called with right data.
        EXPECT_CALL(mock_virtdisk_api, OpenVirtualDisk)
            .WillOnce(DoAll([](PVIRTUAL_STORAGE_TYPE VirtualStorageType,
                               PCWSTR Path,
                               VIRTUAL_DISK_ACCESS_MASK VirtualDiskAccessMask,
                               OPEN_VIRTUAL_DISK_FLAG Flags,
                               POPEN_VIRTUAL_DISK_PARAMETERS Parameters,
                               PHANDLE Handle) { *Handle = mock_handle_object; },
                            Return(ERROR_SUCCESS)));

        EXPECT_CALL(mock_virtdisk_api, ResizeVirtualDisk)
            .WillOnce(DoAll([](HANDLE VirtualDiskHandle,
                               RESIZE_VIRTUAL_DISK_FLAG Flags,
                               PRESIZE_VIRTUAL_DISK_PARAMETERS Parameters,
                               LPOVERLAPPED Overlapped) {},
                            Return(ERROR_INVALID_PARAMETER)));

        EXPECT_CALL(mock_virtdisk_api, CloseHandle(Eq(mock_handle_object))).WillOnce(Return(true));
        logger_scope.mock_logger->expect_log(
            mpl::Level::error,
            "resize_virtual_disk(...) > ResizeVirtualDisk failed with 87!");
    }

    {
        const auto& [status, status_msg] = VirtDisk().resize_virtual_disk("test.vhdx", 1234567);
        EXPECT_FALSE(status.success());
        ASSERT_FALSE(status_msg.empty());
        EXPECT_NE(status_msg.find(L"ResizeVirtualDisk failed"), std::wstring::npos);
        EXPECT_NE(status_msg.find(L"(0x80070057)"), std::wstring::npos);
    }
}

// ---------------------------------------------------------

TEST_F(HyperVVirtDisk_UnitTests, clear_sparse_flag_happy_path)
{
    { // Verify that the dependencies are called with right data.
        EXPECT_CALL(mock_virtdisk_api, GetFileAttributesW(StrEq(L"test.vhdx")))
            .WillOnce(Return(FILE_ATTRIBUTE_ARCHIVE | FILE_ATTRIBUTE_SPARSE_FILE));

        EXPECT_CALL(mock_virtdisk_api, CreateFileW)
            .WillOnce(DoAll(
                [](LPCWSTR lpFileName,
                   DWORD dwDesiredAccess,
                   DWORD dwShareMode,
                   LPSECURITY_ATTRIBUTES lpSecurityAttributes,
                   DWORD dwCreationDisposition,
                   DWORD dwFlagsAndAttributes,
                   HANDLE hTemplateFile) {
                    ASSERT_NE(nullptr, lpFileName);
                    ASSERT_STREQ(lpFileName, L"test.vhdx");
                    ASSERT_EQ(dwDesiredAccess, GENERIC_WRITE);
                    ASSERT_EQ(dwShareMode, FILE_SHARE_READ);
                    ASSERT_EQ(nullptr, lpSecurityAttributes);
                    ASSERT_EQ(dwCreationDisposition, OPEN_EXISTING);
                    ASSERT_EQ(dwFlagsAndAttributes, 0);
                    ASSERT_EQ(nullptr, hTemplateFile);
                },
                Return(mock_handle_object)));

        EXPECT_CALL(mock_virtdisk_api, DeviceIoControl)
            .WillOnce(DoAll(
                [](HANDLE hDevice,
                   DWORD dwIoControlCode,
                   LPVOID lpInBuffer,
                   DWORD nInBufferSize,
                   LPVOID lpOutBuffer,
                   DWORD nOutBufferSize,
                   LPDWORD lpBytesReturned,
                   LPOVERLAPPED lpOverlapped) {
                    ASSERT_EQ(mock_handle_object, hDevice);
                    ASSERT_EQ(dwIoControlCode, FSCTL_SET_SPARSE);
                    ASSERT_NE(nullptr, lpInBuffer);
                    ASSERT_EQ(nInBufferSize, sizeof(FILE_SET_SPARSE_BUFFER));
                    ASSERT_FALSE(static_cast<PFILE_SET_SPARSE_BUFFER>(lpInBuffer)->SetSparse);
                    ASSERT_EQ(nullptr, lpOutBuffer);
                    ASSERT_EQ(nOutBufferSize, 0);
                    ASSERT_NE(nullptr, lpBytesReturned);
                    ASSERT_EQ(nullptr, lpOverlapped);
                },
                Return(TRUE)));

        EXPECT_CALL(mock_virtdisk_api, CloseHandle(Eq(mock_handle_object))).WillOnce(Return(true));
    }

    {
        const auto& [status, status_msg] = VirtDisk().clear_sparse_flag("test.vhdx");
        EXPECT_TRUE(status.success());
        EXPECT_TRUE(status_msg.empty());
    }
}

// ---------------------------------------------------------

TEST_F(HyperVVirtDisk_UnitTests, clear_sparse_flag_not_sparse)
{
    { // Verify that the file is not opened when it is not sparse.
        EXPECT_CALL(mock_virtdisk_api, GetFileAttributesW(StrEq(L"test.vhdx")))
            .WillOnce(Return(FILE_ATTRIBUTE_ARCHIVE));
    }

    {
        const auto& [status, status_msg] = VirtDisk().clear_sparse_flag("test.vhdx");
        EXPECT_TRUE(status.success());
        EXPECT_TRUE(status_msg.empty());
    }
}

// ---------------------------------------------------------

TEST_F(HyperVVirtDisk_UnitTests, clear_sparse_flag_get_attributes_failed)
{
    { // Verify that the dependencies are called with right data.
        EXPECT_CALL(mock_virtdisk_api, GetFileAttributesW(StrEq(L"test.vhdx")))
            .WillOnce(Return(INVALID_FILE_ATTRIBUTES));
        EXPECT_CALL(mock_virtdisk_api, GetLastError).WillOnce(Return(ERROR_FILE_NOT_FOUND));
        logger_scope.mock_logger->expect_log(
            mpl::Level::error,
            "clear_sparse_flag(...) > GetFileAttributesW failed with 2!");
    }

    {
        const auto& [status, status_msg] = VirtDisk().clear_sparse_flag("test.vhdx");
        EXPECT_FALSE(status.success());
        ASSERT_FALSE(status_msg.empty());
        EXPECT_NE(status_msg.find(L"GetFileAttributesW failed"), std::wstring::npos);
        EXPECT_NE(status_msg.find(L"(0x80070002)"), std::wstring::npos);
    }
}

// ---------------------------------------------------------

TEST_F(HyperVVirtDisk_UnitTests, clear_sparse_flag_open_failed)
{
    { // Verify that the dependencies are called with right data.
        EXPECT_CALL(mock_virtdisk_api, GetFileAttributesW(StrEq(L"test.vhdx")))
            .WillOnce(Return(FILE_ATTRIBUTE_SPARSE_FILE));
        EXPECT_CALL(mock_virtdisk_api, CreateFileW).WillOnce(Return(INVALID_HANDLE_VALUE));
        EXPECT_CALL(mock_virtdisk_api, GetLastError).WillOnce(Return(ERROR_ACCESS_DENIED));
        logger_scope.mock_logger->expect_log(mpl::Level::error,
                                             "clear_sparse_flag(...) > CreateFileW failed with 5!");
    }

    {
        const auto& [status, status_msg] = VirtDisk().clear_sparse_flag("test.vhdx");
        EXPECT_FALSE(status.success());
        ASSERT_FALSE(status_msg.empty());
        EXPECT_NE(status_msg.find(L"CreateFileW failed"), std::wstring::npos);
        EXPECT_NE(status_msg.find(L"(0x80070005)"), std::wstring::npos);
    }
}

// ---------------------------------------------------------

TEST_F(HyperVVirtDisk_UnitTests, clear_sparse_flag_ioctl_failed)
{
    { // Verify that the dependencies are called with right data.
        EXPECT_CALL(mock_virtdisk_api, GetFileAttributesW(StrEq(L"test.vhdx")))
            .WillOnce(Return(FILE_ATTRIBUTE_SPARSE_FILE));
        EXPECT_CALL(mock_virtdisk_api, CreateFileW).WillOnce(Return(mock_handle_object));
        logger_scope.mock_logger->expect_log(
            mpl::Level::error,
            "clear_sparse_flag(...) > DeviceIoControl(FSCTL_SET_SPARSE) failed with 1!");

        // The error code must be read before the handle is closed.
        InSequence sequence;
        EXPECT_CALL(mock_virtdisk_api, DeviceIoControl).WillOnce(Return(FALSE));
        EXPECT_CALL(mock_virtdisk_api, GetLastError).WillOnce(Return(ERROR_INVALID_FUNCTION));
        EXPECT_CALL(mock_virtdisk_api, CloseHandle(Eq(mock_handle_object))).WillOnce(Return(true));
    }

    {
        const auto& [status, status_msg] = VirtDisk().clear_sparse_flag("test.vhdx");
        EXPECT_FALSE(status.success());
        ASSERT_FALSE(status_msg.empty());
        EXPECT_NE(status_msg.find(L"DeviceIoControl(FSCTL_SET_SPARSE) failed"), std::wstring::npos);
        EXPECT_NE(status_msg.find(L"(0x80070001)"), std::wstring::npos);
    }
}

// ---------------------------------------------------------

TEST_F(HyperVVirtDisk_UnitTests, get_virtual_disk_info_happy_path)
{
    { // Verify that the dependencies are called with right data.
        EXPECT_CALL(mock_virtdisk_api, OpenVirtualDisk)
            .WillOnce(DoAll(
                [](PVIRTUAL_STORAGE_TYPE VirtualStorageType,
                   PCWSTR Path,
                   VIRTUAL_DISK_ACCESS_MASK VirtualDiskAccessMask,
                   OPEN_VIRTUAL_DISK_FLAG Flags,
                   POPEN_VIRTUAL_DISK_PARAMETERS Parameters,
                   PHANDLE Handle) {
                    ASSERT_NE(nullptr, VirtualStorageType);
                    ASSERT_EQ(VirtualStorageType->DeviceId, VIRTUAL_STORAGE_TYPE_DEVICE_UNKNOWN);
                    ASSERT_EQ(VirtualStorageType->VendorId, VIRTUAL_STORAGE_TYPE_VENDOR_UNKNOWN);
                    ASSERT_NE(nullptr, Path);
                    ASSERT_STREQ(Path, L"test.vhdx");
                    ASSERT_EQ(VIRTUAL_DISK_ACCESS_ALL, VirtualDiskAccessMask);
                    ASSERT_EQ(OPEN_VIRTUAL_DISK_FLAG_NONE, Flags);
                    ASSERT_EQ(nullptr, Parameters);
                    ASSERT_NE(nullptr, Handle);
                    ASSERT_EQ(nullptr, *Handle);

                    *Handle = mock_handle_object;
                },
                Return(ERROR_SUCCESS)));

        // The API will be called for several times.
        EXPECT_CALL(mock_virtdisk_api, GetVirtualDiskInformation)
            .WillOnce(DoAll(
                [](HANDLE VirtualDiskHandle,
                   PULONG VirtualDiskInfoSize,
                   PGET_VIRTUAL_DISK_INFO VirtualDiskInfo,
                   PULONG SizeUsed) {
                    ASSERT_EQ(mock_handle_object, VirtualDiskHandle);
                    ASSERT_NE(nullptr, VirtualDiskInfoSize);
                    ASSERT_EQ(sizeof(GET_VIRTUAL_DISK_INFO), *VirtualDiskInfoSize);
                    ASSERT_NE(nullptr, VirtualDiskInfo);
                    ASSERT_EQ(nullptr, SizeUsed);
                    ASSERT_EQ(GET_VIRTUAL_DISK_INFO_SIZE, VirtualDiskInfo->Version);
                    VirtualDiskInfo->Size.VirtualSize = 1111111;
                    VirtualDiskInfo->Size.BlockSize = 2222222;
                    VirtualDiskInfo->Size.PhysicalSize = 3333333;
                    VirtualDiskInfo->Size.SectorSize = 4444444;
                },
                Return(ERROR_SUCCESS)))
            .WillOnce(DoAll(
                [](HANDLE VirtualDiskHandle,
                   PULONG VirtualDiskInfoSize,
                   PGET_VIRTUAL_DISK_INFO VirtualDiskInfo,
                   PULONG SizeUsed) {
                    ASSERT_EQ(mock_handle_object, VirtualDiskHandle);
                    ASSERT_NE(nullptr, VirtualDiskInfoSize);
                    ASSERT_EQ(sizeof(GET_VIRTUAL_DISK_INFO), *VirtualDiskInfoSize);
                    ASSERT_NE(nullptr, VirtualDiskInfo);
                    ASSERT_EQ(nullptr, SizeUsed);
                    ASSERT_EQ(GET_VIRTUAL_DISK_INFO_VIRTUAL_STORAGE_TYPE, VirtualDiskInfo->Version);
                    VirtualDiskInfo->VirtualStorageType.DeviceId = VIRTUAL_STORAGE_TYPE_DEVICE_VHDX;
                    VirtualDiskInfo->VirtualStorageType.VendorId =
                        VIRTUAL_STORAGE_TYPE_VENDOR_UNKNOWN;
                },
                Return(ERROR_SUCCESS)));

        EXPECT_CALL(mock_virtdisk_api, CloseHandle(Eq(mock_handle_object))).WillOnce(Return(true));
    }

    {
        hyperv::virtdisk::VirtualDiskInfo info{};
        const auto& [status, status_msg] = VirtDisk().get_virtual_disk_info("test.vhdx", info);
        ASSERT_TRUE(status.success());
        ASSERT_TRUE(status_msg.empty());

        ASSERT_TRUE(info.size.has_value());
        // ASSERT_TRUE(info.smallest_safe_virtual_size.has_value());
        ASSERT_TRUE(info.virtual_storage_type.has_value());

        ASSERT_EQ(info.size->virtual_, 1111111);
        ASSERT_EQ(info.size->block, 2222222);
        ASSERT_EQ(info.size->physical, 3333333);
        ASSERT_EQ(info.size->sector, 4444444);

        ASSERT_STREQ(info.virtual_storage_type.value().c_str(), "vhdx");
        // ASSERT_EQ(info.smallest_safe_virtual_size.value(), 123456);
    }
}

// ---------------------------------------------------------

TEST_F(HyperVVirtDisk_UnitTests, get_virtual_disk_info_fail_some)
{
    { // Verify that the dependencies are called with right data.
        EXPECT_CALL(mock_virtdisk_api, OpenVirtualDisk)
            .WillOnce(DoAll(
                [](PVIRTUAL_STORAGE_TYPE VirtualStorageType,
                   PCWSTR Path,
                   VIRTUAL_DISK_ACCESS_MASK VirtualDiskAccessMask,
                   OPEN_VIRTUAL_DISK_FLAG Flags,
                   POPEN_VIRTUAL_DISK_PARAMETERS Parameters,
                   PHANDLE Handle) {
                    ASSERT_NE(nullptr, VirtualStorageType);
                    ASSERT_EQ(VirtualStorageType->DeviceId, VIRTUAL_STORAGE_TYPE_DEVICE_UNKNOWN);
                    ASSERT_EQ(VirtualStorageType->VendorId, VIRTUAL_STORAGE_TYPE_VENDOR_UNKNOWN);
                    ASSERT_NE(nullptr, Path);
                    ASSERT_STREQ(Path, L"test.vhdx");
                    ASSERT_EQ(VIRTUAL_DISK_ACCESS_ALL, VirtualDiskAccessMask);
                    ASSERT_EQ(OPEN_VIRTUAL_DISK_FLAG_NONE, Flags);
                    ASSERT_EQ(nullptr, Parameters);
                    ASSERT_NE(nullptr, Handle);
                    ASSERT_EQ(nullptr, *Handle);

                    *Handle = mock_handle_object;
                },
                Return(ERROR_SUCCESS)));

        // The API will be called for several times.
        EXPECT_CALL(mock_virtdisk_api, GetVirtualDiskInformation)
            .WillOnce(DoAll(
                [](HANDLE VirtualDiskHandle,
                   PULONG VirtualDiskInfoSize,
                   PGET_VIRTUAL_DISK_INFO VirtualDiskInfo,
                   PULONG SizeUsed) {
                    ASSERT_EQ(mock_handle_object, VirtualDiskHandle);
                    ASSERT_NE(nullptr, VirtualDiskInfoSize);
                    ASSERT_EQ(sizeof(GET_VIRTUAL_DISK_INFO), *VirtualDiskInfoSize);
                    ASSERT_NE(nullptr, VirtualDiskInfo);
                    ASSERT_EQ(nullptr, SizeUsed);
                    ASSERT_EQ(GET_VIRTUAL_DISK_INFO_SIZE, VirtualDiskInfo->Version);
                    VirtualDiskInfo->Size.VirtualSize = 1111111;
                    VirtualDiskInfo->Size.BlockSize = 2222222;
                    VirtualDiskInfo->Size.PhysicalSize = 3333333;
                    VirtualDiskInfo->Size.SectorSize = 4444444;
                },
                Return(ERROR_SUCCESS)))
            .WillOnce(Return(ERROR_INVALID_PARAMETER));

        EXPECT_CALL(mock_virtdisk_api, CloseHandle(Eq(mock_handle_object))).WillOnce(Return(true));
        logger_scope.mock_logger->expect_log(mpl::Level::warning,
                                             "get_virtual_disk_info(...) > failed to get 6");
    }

    {
        hyperv::virtdisk::VirtualDiskInfo info{};
        const auto& [status, status_msg] = VirtDisk().get_virtual_disk_info("test.vhdx", info);
        ASSERT_TRUE(status.success());
        ASSERT_TRUE(status_msg.empty());

        ASSERT_TRUE(info.size.has_value());
        ASSERT_FALSE(info.virtual_storage_type.has_value());
        // ASSERT_TRUE(info.smallest_safe_virtual_size.has_value());

        ASSERT_EQ(info.size->virtual_, 1111111);
        ASSERT_EQ(info.size->block, 2222222);
        ASSERT_EQ(info.size->physical, 3333333);
        ASSERT_EQ(info.size->sector, 4444444);

        // ASSERT_EQ(info.smallest_safe_virtual_size.value(), 123456);
    }
}

// ---------------------------------------------------------

TEST_F(HyperVVirtDisk_UnitTests, reparent_virtual_disk_happy_path)
{
    { // Verify that the dependencies are called with right data.

        EXPECT_CALL(mock_virtdisk_api, OpenVirtualDisk)
            .WillOnce(DoAll(
                [](PVIRTUAL_STORAGE_TYPE VirtualStorageType,
                   PCWSTR Path,
                   VIRTUAL_DISK_ACCESS_MASK VirtualDiskAccessMask,
                   OPEN_VIRTUAL_DISK_FLAG Flags,
                   POPEN_VIRTUAL_DISK_PARAMETERS Parameters,
                   PHANDLE Handle) {
                    ASSERT_NE(nullptr, VirtualStorageType);
                    EXPECT_EQ(VirtualStorageType->DeviceId, VIRTUAL_STORAGE_TYPE_DEVICE_UNKNOWN);
                    EXPECT_EQ(VirtualStorageType->VendorId, VIRTUAL_STORAGE_TYPE_VENDOR_UNKNOWN);
                    EXPECT_STREQ(Path, L"child.avhdx");
                    EXPECT_EQ(VIRTUAL_DISK_ACCESS_NONE, VirtualDiskAccessMask);
                    EXPECT_EQ(OPEN_VIRTUAL_DISK_FLAG_NO_PARENTS, Flags);
                    ASSERT_NE(nullptr, Parameters);
                    EXPECT_EQ(Parameters->Version, OPEN_VIRTUAL_DISK_VERSION_2);
                    EXPECT_EQ(Parameters->Version2.GetInfoOnly, false);
                    ASSERT_NE(nullptr, Handle);
                    ASSERT_EQ(nullptr, *Handle);
                    *Handle = mock_handle_object;
                },
                Return(ERROR_SUCCESS)));

        EXPECT_CALL(mock_virtdisk_api, SetVirtualDiskInformation)
            .WillOnce(DoAll(
                [](HANDLE VirtualDiskHandle, PSET_VIRTUAL_DISK_INFO VirtualDiskInfo) {
                    ASSERT_EQ(VirtualDiskHandle, mock_handle_object);
                    ASSERT_NE(nullptr, VirtualDiskInfo);
                    EXPECT_EQ(VirtualDiskInfo->Version,
                              SET_VIRTUAL_DISK_INFO_PARENT_PATH_WITH_DEPTH);
                    EXPECT_STREQ(VirtualDiskInfo->ParentPathWithDepthInfo.ParentFilePath,
                                 L"parent.vhdx");
                    EXPECT_EQ(VirtualDiskInfo->ParentPathWithDepthInfo.ChildDepth, 1);
                },
                Return(ERROR_SUCCESS)));

        EXPECT_CALL(mock_virtdisk_api, CloseHandle(Eq(mock_handle_object))).WillOnce(Return(true));
    }

    {
        const auto& [status, status_msg] = VirtDisk().reparent_virtual_disk("child.avhdx",
                                                                            "parent.vhdx");
        EXPECT_TRUE(status.success());
        EXPECT_TRUE(status_msg.empty());
    }
}

// ---------------------------------------------------------

TEST_F(HyperVVirtDisk_UnitTests, reparent_virtual_disk_open_disk_failure)
{

    { // Verify that the dependencies are called with right data.
        open_vhd_expect_failure();
    }

    {
        const auto& [status, status_msg] = VirtDisk().reparent_virtual_disk("child.avhdx",
                                                                            "parent.vhdx");
        EXPECT_FALSE(status.success());
        EXPECT_FALSE(status_msg.empty());
    }
}

// ---------------------------------------------------------

TEST_F(HyperVVirtDisk_UnitTests, merge_virtual_disk_happy_path)
{
    { // Verify that the dependencies are called with right data.

        EXPECT_CALL(mock_virtdisk_api, OpenVirtualDisk)
            .WillOnce(DoAll(
                [](PVIRTUAL_STORAGE_TYPE VirtualStorageType,
                   PCWSTR Path,
                   VIRTUAL_DISK_ACCESS_MASK VirtualDiskAccessMask,
                   OPEN_VIRTUAL_DISK_FLAG Flags,
                   POPEN_VIRTUAL_DISK_PARAMETERS Parameters,
                   PHANDLE Handle) {
                    ASSERT_NE(nullptr, VirtualStorageType);
                    EXPECT_EQ(VirtualStorageType->DeviceId, VIRTUAL_STORAGE_TYPE_DEVICE_UNKNOWN);
                    EXPECT_EQ(VirtualStorageType->VendorId, VIRTUAL_STORAGE_TYPE_VENDOR_UNKNOWN);
                    EXPECT_STREQ(Path, L"child.avhdx");
                    EXPECT_EQ(VIRTUAL_DISK_ACCESS_METAOPS | VIRTUAL_DISK_ACCESS_GET_INFO,
                              VirtualDiskAccessMask);
                    EXPECT_EQ(OPEN_VIRTUAL_DISK_FLAG_NONE, Flags);
                    ASSERT_NE(nullptr, Parameters);
                    EXPECT_EQ(Parameters->Version, OPEN_VIRTUAL_DISK_VERSION_1);
                    EXPECT_EQ(Parameters->Version1.RWDepth, 2);
                    ASSERT_NE(nullptr, Handle);
                    ASSERT_EQ(nullptr, *Handle);
                    *Handle = mock_handle_object;
                },
                Return(ERROR_SUCCESS)));

        EXPECT_CALL(mock_virtdisk_api, MergeVirtualDisk)
            .WillOnce(DoAll(
                [](HANDLE VirtualDiskHandle,
                   MERGE_VIRTUAL_DISK_FLAG Flags,
                   PMERGE_VIRTUAL_DISK_PARAMETERS Parameters,
                   LPOVERLAPPED Overlapped) {
                    ASSERT_EQ(VirtualDiskHandle, mock_handle_object);
                    EXPECT_EQ(MERGE_VIRTUAL_DISK_FLAG_NONE, Flags);
                    ASSERT_NE(nullptr, Parameters);
                    EXPECT_EQ(MERGE_VIRTUAL_DISK_VERSION_1, Parameters->Version);
                    EXPECT_EQ(MERGE_VIRTUAL_DISK_DEFAULT_MERGE_DEPTH,
                              Parameters->Version1.MergeDepth);
                    ASSERT_EQ(nullptr, Overlapped);
                },
                Return(ERROR_SUCCESS)));

        EXPECT_CALL(mock_virtdisk_api, CloseHandle(Eq(mock_handle_object))).WillOnce(Return(true));
    }

    {
        const auto& [status, status_msg] = VirtDisk().merge_virtual_disk_into_parent("child.avhdx");
        EXPECT_TRUE(status.success());
        EXPECT_TRUE(status_msg.empty());
    }
}

// ---------------------------------------------------------

TEST_F(HyperVVirtDisk_UnitTests, merge_virtual_disk_open_disk_failure)
{

    { // Verify that the dependencies are called with right data.
        open_vhd_expect_failure();
    }

    {
        const auto& [status, status_msg] = VirtDisk().merge_virtual_disk_into_parent("child.avhdx");
        EXPECT_FALSE(status.success());
        EXPECT_FALSE(status_msg.empty());
    }
}

} // namespace multipass::test
