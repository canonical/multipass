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
#include "tests/mock_file_ops.h"
#include "tests/mock_logger.h"

#include <fmt/xchar.h>

#include <src/platform/backends/hyperv_api/virtdisk/virtdisk_api_wrapper.h>
#include <src/platform/backends/hyperv_api/virtdisk/virtdisk_disk_info.h>

namespace mpt = multipass::test;
namespace mpl = multipass::logging;

using namespace testing;

namespace multipass::test
{

using uut_t = hyperv::virtdisk::VirtDiskWrapper;

struct HyperVVirtDisk_UnitTests : public ::testing::Test
{
    mpt::MockLogger::Scope logger_scope = mpt::MockLogger::inject();

    virtual void SetUp() override
    {

        // Each of the unit tests are expected to have their own mock functions
        // and override the mock_api_table with them. Hence, the stub mocks should
        // not be called at all.
        // If any of them do get called, then:
        //
        // a-) You have forgotten to mock something
        // b-) The implementation is using a function that you didn't expect
        //
        // Either way, you should have a look.
        EXPECT_NO_CALL(stub_mock_create_virtual_disk);
        EXPECT_NO_CALL(stub_mock_open_virtual_disk);
        EXPECT_NO_CALL(stub_mock_resize_virtual_disk);
        EXPECT_NO_CALL(stub_mock_get_virtual_disk_information);
        EXPECT_NO_CALL(stub_mock_close_handle);
    }

    // Set of placeholder mocks in order to catch *unexpected* calls.
    ::testing::MockFunction<decltype(::CreateVirtualDisk)> stub_mock_create_virtual_disk;
    ::testing::MockFunction<decltype(::OpenVirtualDisk)> stub_mock_open_virtual_disk;
    ::testing::MockFunction<decltype(::ResizeVirtualDisk)> stub_mock_resize_virtual_disk;
    ::testing::MockFunction<decltype(::MergeVirtualDisk)> stub_mock_merge_virtual_disk;
    ::testing::MockFunction<decltype(::GetVirtualDiskInformation)> stub_mock_get_virtual_disk_information;
    ::testing::MockFunction<decltype(::SetVirtualDiskInformation)> stub_mock_set_virtual_disk_information;
    ::testing::MockFunction<decltype(::CloseHandle)> stub_mock_close_handle;

    // Initialize the API table with stub functions, so if any of these fire without
    // our will, we'll know.
    hyperv::virtdisk::VirtDiskAPITable mock_api_table{
        stub_mock_create_virtual_disk.AsStdFunction(),
        stub_mock_open_virtual_disk.AsStdFunction(),
        stub_mock_resize_virtual_disk.AsStdFunction(),
        stub_mock_merge_virtual_disk.AsStdFunction(),
        stub_mock_get_virtual_disk_information.AsStdFunction(),
        stub_mock_set_virtual_disk_information.AsStdFunction(),
        stub_mock_close_handle.AsStdFunction(),
    };

    // Sentinel values as mock API parameters. These handles are opaque handles and
    // they're not being dereferenced in any way -- only address values are compared.
    inline static auto mock_handle_object = reinterpret_cast<HANDLE>(0xbadf00d);
};

// ---------------------------------------------------------

TEST_F(HyperVVirtDisk_UnitTests, create_virtual_disk_vhdx_happy_path)
{
    /******************************************************
     * Override the default mock functions.
     ******************************************************/
    ::testing::MockFunction<decltype(::CreateVirtualDisk)> mock_create_virtual_disk;
    ::testing::MockFunction<decltype(::CloseHandle)> mock_close_handle;

    mock_api_table.CreateVirtualDisk = mock_create_virtual_disk.AsStdFunction();
    mock_api_table.CloseHandle = mock_close_handle.AsStdFunction();

    /******************************************************
     * Verify that the dependencies are called with right
     * data.
     ******************************************************/
    {
        EXPECT_CALL(mock_create_virtual_disk, Call)
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

        EXPECT_CALL(mock_close_handle, Call(Eq(mock_handle_object))).WillOnce(Return(true));

        logger_scope.mock_logger->expect_log(mpl::Level::debug, "VirtDiskWrapper::VirtDiskWrapper(...)");
        logger_scope.mock_logger->expect_log(
            mpl::Level::debug,
            "create_virtual_disk(...) > params: Size (in bytes): (2097152) | Path: (test.vhdx)");
    }

    hyperv::virtdisk::CreateVirtualDiskParameters params{};
    params.path = "test.vhdx";
    params.size_in_bytes = 2097152;

    {
        uut_t uut{mock_api_table};
        const auto& [status, status_msg] = uut.create_virtual_disk(params);
        EXPECT_TRUE(status);
        EXPECT_TRUE(status_msg.empty());
    }
}

// ---------------------------------------------------------

TEST_F(HyperVVirtDisk_UnitTests, create_virtual_disk_vhd_happy_path)
{
    /******************************************************
     * Override the default mock functions.
     ******************************************************/
    ::testing::MockFunction<decltype(::CreateVirtualDisk)> mock_create_virtual_disk;
    ::testing::MockFunction<decltype(::CloseHandle)> mock_close_handle;

    mock_api_table.CreateVirtualDisk = mock_create_virtual_disk.AsStdFunction();
    mock_api_table.CloseHandle = mock_close_handle.AsStdFunction();

    /******************************************************
     * Verify that the dependencies are called with right
     * data.
     ******************************************************/
    {
        EXPECT_CALL(mock_create_virtual_disk, Call)
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

        EXPECT_CALL(mock_close_handle, Call(Eq(mock_handle_object))).WillOnce(Return(true));

        logger_scope.mock_logger->expect_log(mpl::Level::debug, "VirtDiskWrapper::VirtDiskWrapper(...)");
        logger_scope.mock_logger->expect_log(
            mpl::Level::debug,
            "create_virtual_disk(...) > params: Size (in bytes): (2097152) | Path: (test.vhd)");
    }

    hyperv::virtdisk::CreateVirtualDiskParameters params{};
    params.path = "test.vhd";
    params.size_in_bytes = 2097152;

    {
        uut_t uut{mock_api_table};
        const auto& [status, status_msg] = uut.create_virtual_disk(params);
        EXPECT_TRUE(status);
        EXPECT_TRUE(status_msg.empty());
    }
}

// ---------------------------------------------------------

TEST_F(HyperVVirtDisk_UnitTests, create_virtual_disk_vhdx_with_source)
{
    /******************************************************
     * Override the default mock functions.
     ******************************************************/
    ::testing::MockFunction<decltype(::CreateVirtualDisk)> mock_create_virtual_disk;
    ::testing::MockFunction<decltype(::OpenVirtualDisk)> mock_open_virtual_disk;
    ::testing::MockFunction<decltype(::GetVirtualDiskInformation)> mock_get_virtual_disk_information;
    ::testing::MockFunction<decltype(::CloseHandle)> mock_close_handle;

    mock_api_table.CreateVirtualDisk = mock_create_virtual_disk.AsStdFunction();
    mock_api_table.OpenVirtualDisk = mock_open_virtual_disk.AsStdFunction();
    mock_api_table.GetVirtualDiskInformation = mock_get_virtual_disk_information.AsStdFunction();
    mock_api_table.CloseHandle = mock_close_handle.AsStdFunction();

    auto [mock_file_ops, guard] = mpt::MockFileOps::inject();
    EXPECT_CALL(*mock_file_ops, exists(TypedEq<const fs::path&>(L"source.vhdx"))).WillOnce(Return(true));

    /******************************************************
     * Verify that the dependencies are called with right
     * data.
     ******************************************************/
    {
        EXPECT_CALL(mock_create_virtual_disk, Call)
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
                    ASSERT_EQ(Parameters->Version2.BlockSizeInBytes, CREATE_VIRTUAL_DISK_PARAMETERS_DEFAULT_BLOCK_SIZE);
                    ASSERT_STREQ(Parameters->Version2.SourcePath, L"source.vhdx");
                    ASSERT_EQ(Parameters->Version2.SourceVirtualStorageType.DeviceId, VIRTUAL_STORAGE_TYPE_DEVICE_VHDX);
                    ASSERT_EQ(Parameters->Version2.SourceVirtualStorageType.VendorId,
                              VIRTUAL_STORAGE_TYPE_VENDOR_UNKNOWN);

                    ASSERT_EQ(nullptr, Overlapped);
                    ASSERT_NE(nullptr, Handle);
                    ASSERT_EQ(nullptr, *Handle);

                    *Handle = mock_handle_object;
                },
                Return(ERROR_SUCCESS)));

        EXPECT_CALL(mock_open_virtual_disk, Call)
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
        EXPECT_CALL(mock_get_virtual_disk_information, Call)
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
                    VirtualDiskInfo->VirtualStorageType.VendorId = VIRTUAL_STORAGE_TYPE_VENDOR_UNKNOWN;
                    VirtualDiskInfo->SmallestSafeVirtualSize = 123456;
                    VirtualDiskInfo->Size.VirtualSize = 1111111;
                    VirtualDiskInfo->Size.BlockSize = 2222222;
                    VirtualDiskInfo->Size.PhysicalSize = 3333333;
                    VirtualDiskInfo->Size.SectorSize = 4444444;
                    VirtualDiskInfo->ProviderSubtype = 3; // dynamic
                },
                Return(ERROR_SUCCESS)));

        EXPECT_CALL(mock_close_handle, Call(Eq(mock_handle_object))).Times(2).WillRepeatedly(Return(true));

        logger_scope.mock_logger->expect_log(mpl::Level::debug, "VirtDiskWrapper::VirtDiskWrapper(...)");
        logger_scope.mock_logger->expect_log(
            mpl::Level::debug,
            "create_virtual_disk(...) > params: Size (in bytes): (0) | Path: (test.vhdx)");
        logger_scope.mock_logger->expect_log(mpl::Level::debug, "open_virtual_disk(...) > vhdx_path: source.vhdx");
        logger_scope.mock_logger->expect_log(mpl::Level::debug, "get_virtual_disk_info(...) > vhdx_path: source.vhdx");
        logger_scope.mock_logger->expect_log(mpl::Level::debug,
                                             "create_virtual_disk(...) > source disk info fetch result");
        logger_scope.mock_logger->expect_log(mpl::Level::debug, "create_virtual_disk(...) > cloning");
    }

    hyperv::virtdisk::CreateVirtualDiskParameters params{};
    params.predecessor = hyperv::virtdisk::SourcePathParameters{"source.vhdx"};
    params.path = "test.vhdx";
    params.size_in_bytes = 0;

    {
        uut_t uut{mock_api_table};
        const auto& [status, status_msg] = uut.create_virtual_disk(params);
        EXPECT_TRUE(status);
        EXPECT_TRUE(status_msg.empty());
    }
}

// ---------------------------------------------------------

TEST_F(HyperVVirtDisk_UnitTests, create_virtual_disk_failed)
{
    /******************************************************
     * Override the default mock functions.
     ******************************************************/
    ::testing::MockFunction<decltype(::CreateVirtualDisk)> mock_create_virtual_disk;

    mock_api_table.CreateVirtualDisk = mock_create_virtual_disk.AsStdFunction();

    /******************************************************
     * Verify that the dependencies are called with right
     * data.
     ******************************************************/
    {
        EXPECT_CALL(mock_create_virtual_disk, Call)
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

        logger_scope.mock_logger->expect_log(mpl::Level::debug, "VirtDiskWrapper::VirtDiskWrapper(...)");
        logger_scope.mock_logger->expect_log(
            mpl::Level::debug,
            "create_virtual_disk(...) > params: Size (in bytes): (2097152) | Path: (test.vhd)");
        logger_scope.mock_logger->expect_log(mpl::Level::error,
                                             "create_virtual_disk(...) > CreateVirtualDisk failed with 3!");
    }

    hyperv::virtdisk::CreateVirtualDiskParameters params{};
    params.path = "test.vhd";
    params.size_in_bytes = 2097152;

    {
        uut_t uut{mock_api_table};
        const auto& [status, status_msg] = uut.create_virtual_disk(params);
        EXPECT_FALSE(status);
        ASSERT_FALSE(status_msg.empty());
        ASSERT_STREQ(status_msg.c_str(), L"CreateVirtualDisk failed with 3!");
    }
}

// ---------------------------------------------------------

TEST_F(HyperVVirtDisk_UnitTests, resize_virtual_disk_happy_path)
{
    /******************************************************
     * Override the default mock functions.
     ******************************************************/
    ::testing::MockFunction<decltype(::OpenVirtualDisk)> mock_open_virtual_disk;
    ::testing::MockFunction<decltype(::ResizeVirtualDisk)> mock_resize_virtual_disk;
    ::testing::MockFunction<decltype(::CloseHandle)> mock_close_handle;

    mock_api_table.OpenVirtualDisk = mock_open_virtual_disk.AsStdFunction();
    mock_api_table.ResizeVirtualDisk = mock_resize_virtual_disk.AsStdFunction();
    mock_api_table.CloseHandle = mock_close_handle.AsStdFunction();

    /******************************************************
     * Verify that the dependencies are called with right
     * data.
     ******************************************************/
    {
        EXPECT_CALL(mock_open_virtual_disk, Call)
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

        EXPECT_CALL(mock_resize_virtual_disk, Call)
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

        EXPECT_CALL(mock_close_handle, Call(Eq(mock_handle_object))).WillOnce(Return(true));
        logger_scope.mock_logger->expect_log(mpl::Level::debug, "VirtDiskWrapper::VirtDiskWrapper(...)");
        logger_scope.mock_logger->expect_log(
            mpl::Level::debug,
            "resize_virtual_disk(...) > vhdx_path: test.vhdx, new_size_bytes: 1234567");
        logger_scope.mock_logger->expect_log(mpl::Level::debug, "open_virtual_disk(...) > vhdx_path: test.vhdx");
    }

    {
        uut_t uut{mock_api_table};
        const auto& [status, status_msg] = uut.resize_virtual_disk("test.vhdx", 1234567);
        EXPECT_TRUE(status);
        EXPECT_TRUE(status_msg.empty());
    }
}

// ---------------------------------------------------------

TEST_F(HyperVVirtDisk_UnitTests, resize_virtual_disk_open_failed)
{
    /******************************************************
     * Override the default mock functions.
     ******************************************************/
    ::testing::MockFunction<decltype(::OpenVirtualDisk)> mock_open_virtual_disk;

    mock_api_table.OpenVirtualDisk = mock_open_virtual_disk.AsStdFunction();

    /******************************************************
     * Verify that the dependencies are called with right
     * data.
     ******************************************************/
    {
        EXPECT_CALL(mock_open_virtual_disk, Call)
            .WillOnce(DoAll([](PVIRTUAL_STORAGE_TYPE VirtualStorageType,
                               PCWSTR Path,
                               VIRTUAL_DISK_ACCESS_MASK VirtualDiskAccessMask,
                               OPEN_VIRTUAL_DISK_FLAG Flags,
                               POPEN_VIRTUAL_DISK_PARAMETERS Parameters,
                               PHANDLE Handle) {},
                            Return(ERROR_PATH_NOT_FOUND)));
        logger_scope.mock_logger->expect_log(mpl::Level::debug, "VirtDiskWrapper::VirtDiskWrapper(...)");
        logger_scope.mock_logger->expect_log(
            mpl::Level::debug,
            "resize_virtual_disk(...) > vhdx_path: test.vhdx, new_size_bytes: 1234567");
        logger_scope.mock_logger->expect_log(mpl::Level::debug, "open_virtual_disk(...) > vhdx_path: test.vhdx");
        logger_scope.mock_logger->expect_log(mpl::Level::error, "open_virtual_disk(...) > OpenVirtualDisk failed with");
    }

    {
        uut_t uut{mock_api_table};
        const auto& [status, status_msg] = uut.resize_virtual_disk("test.vhdx", 1234567);
        EXPECT_FALSE(status);
        ASSERT_FALSE(status_msg.empty());
        ASSERT_STREQ(status_msg.c_str(), L"open_virtual_disk failed!");
    }
}

// ---------------------------------------------------------

TEST_F(HyperVVirtDisk_UnitTests, resize_virtual_disk_resize_failed)
{
    /******************************************************
     * Override the default mock functions.
     ******************************************************/
    ::testing::MockFunction<decltype(::OpenVirtualDisk)> mock_open_virtual_disk;
    ::testing::MockFunction<decltype(::ResizeVirtualDisk)> mock_resize_virtual_disk;
    ::testing::MockFunction<decltype(::CloseHandle)> mock_close_handle;

    mock_api_table.OpenVirtualDisk = mock_open_virtual_disk.AsStdFunction();
    mock_api_table.ResizeVirtualDisk = mock_resize_virtual_disk.AsStdFunction();
    mock_api_table.CloseHandle = mock_close_handle.AsStdFunction();

    /******************************************************
     * Verify that the dependencies are called with right
     * data.
     ******************************************************/
    {
        EXPECT_CALL(mock_open_virtual_disk, Call)
            .WillOnce(DoAll([](PVIRTUAL_STORAGE_TYPE VirtualStorageType,
                               PCWSTR Path,
                               VIRTUAL_DISK_ACCESS_MASK VirtualDiskAccessMask,
                               OPEN_VIRTUAL_DISK_FLAG Flags,
                               POPEN_VIRTUAL_DISK_PARAMETERS Parameters,
                               PHANDLE Handle) { *Handle = mock_handle_object; },
                            Return(ERROR_SUCCESS)));

        EXPECT_CALL(mock_resize_virtual_disk, Call)
            .WillOnce(DoAll([](HANDLE VirtualDiskHandle,
                               RESIZE_VIRTUAL_DISK_FLAG Flags,
                               PRESIZE_VIRTUAL_DISK_PARAMETERS Parameters,
                               LPOVERLAPPED Overlapped) {},
                            Return(ERROR_INVALID_PARAMETER)));

        EXPECT_CALL(mock_close_handle, Call(Eq(mock_handle_object))).WillOnce(Return(true));
        logger_scope.mock_logger->expect_log(mpl::Level::debug, "VirtDiskWrapper::VirtDiskWrapper(...)");
        logger_scope.mock_logger->expect_log(
            mpl::Level::debug,
            "resize_virtual_disk(...) > vhdx_path: test.vhdx, new_size_bytes: 1234567");
        logger_scope.mock_logger->expect_log(mpl::Level::debug, "open_virtual_disk(...) > vhdx_path: test.vhdx");
        logger_scope.mock_logger->expect_log(mpl::Level::error,
                                             "resize_virtual_disk(...) > ResizeVirtualDisk failed with 87!");
    }

    {
        uut_t uut{mock_api_table};
        const auto& [status, status_msg] = uut.resize_virtual_disk("test.vhdx", 1234567);
        EXPECT_FALSE(status);
        ASSERT_FALSE(status_msg.empty());
        ASSERT_STREQ(status_msg.c_str(), L"ResizeVirtualDisk failed with 87!");
    }
}

// ---------------------------------------------------------

TEST_F(HyperVVirtDisk_UnitTests, get_virtual_disk_info_happy_path)
{
    /******************************************************
     * Override the default mock functions.
     ******************************************************/
    ::testing::MockFunction<decltype(::OpenVirtualDisk)> mock_open_virtual_disk;
    ::testing::MockFunction<decltype(::GetVirtualDiskInformation)> mock_get_virtual_disk_information;
    ::testing::MockFunction<decltype(::CloseHandle)> mock_close_handle;

    mock_api_table.OpenVirtualDisk = mock_open_virtual_disk.AsStdFunction();
    mock_api_table.GetVirtualDiskInformation = mock_get_virtual_disk_information.AsStdFunction();
    mock_api_table.CloseHandle = mock_close_handle.AsStdFunction();

    /******************************************************
     * Verify that the dependencies are called with right
     * data.
     ******************************************************/
    {
        EXPECT_CALL(mock_open_virtual_disk, Call)
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
        EXPECT_CALL(mock_get_virtual_disk_information, Call)
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
                    VirtualDiskInfo->VirtualStorageType.VendorId = VIRTUAL_STORAGE_TYPE_VENDOR_UNKNOWN;
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
                    ASSERT_EQ(GET_VIRTUAL_DISK_INFO_SMALLEST_SAFE_VIRTUAL_SIZE, VirtualDiskInfo->Version);
                    VirtualDiskInfo->SmallestSafeVirtualSize = 123456;
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
                    ASSERT_EQ(GET_VIRTUAL_DISK_INFO_PROVIDER_SUBTYPE, VirtualDiskInfo->Version);
                    VirtualDiskInfo->ProviderSubtype = 3; // dynamic
                },
                Return(ERROR_SUCCESS)));

        EXPECT_CALL(mock_close_handle, Call(Eq(mock_handle_object))).WillOnce(Return(true));
        logger_scope.mock_logger->expect_log(mpl::Level::debug, "VirtDiskWrapper::VirtDiskWrapper(...)");
        logger_scope.mock_logger->expect_log(mpl::Level::debug, "get_virtual_disk_info(...) > vhdx_path: test.vhdx");
        logger_scope.mock_logger->expect_log(mpl::Level::debug, "open_virtual_disk(...) > vhdx_path: test.vhdx");
    }

    {
        uut_t uut{mock_api_table};
        hyperv::virtdisk::VirtualDiskInfo info{};
        const auto& [status, status_msg] = uut.get_virtual_disk_info("test.vhdx", info);
        ASSERT_TRUE(status);
        ASSERT_TRUE(status_msg.empty());

        ASSERT_TRUE(info.size.has_value());
        ASSERT_TRUE(info.smallest_safe_virtual_size.has_value());
        ASSERT_TRUE(info.provider_subtype.has_value());
        ASSERT_TRUE(info.virtual_storage_type.has_value());

        ASSERT_EQ(info.size->virtual_, 1111111);
        ASSERT_EQ(info.size->block, 2222222);
        ASSERT_EQ(info.size->physical, 3333333);
        ASSERT_EQ(info.size->sector, 4444444);

        ASSERT_STREQ(info.virtual_storage_type.value().c_str(), "vhdx");
        ASSERT_EQ(info.smallest_safe_virtual_size.value(), 123456);
        ASSERT_STREQ(info.provider_subtype.value().c_str(), "dynamic");
    }
}

// ---------------------------------------------------------

TEST_F(HyperVVirtDisk_UnitTests, get_virtual_disk_info_fail_some)
{
    /******************************************************
     * Override the default mock functions.
     ******************************************************/
    ::testing::MockFunction<decltype(::OpenVirtualDisk)> mock_open_virtual_disk;
    ::testing::MockFunction<decltype(::GetVirtualDiskInformation)> mock_get_virtual_disk_information;
    ::testing::MockFunction<decltype(::CloseHandle)> mock_close_handle;

    mock_api_table.OpenVirtualDisk = mock_open_virtual_disk.AsStdFunction();
    mock_api_table.GetVirtualDiskInformation = mock_get_virtual_disk_information.AsStdFunction();
    mock_api_table.CloseHandle = mock_close_handle.AsStdFunction();

    /******************************************************
     * Verify that the dependencies are called with right
     * data.
     ******************************************************/
    {
        EXPECT_CALL(mock_open_virtual_disk, Call)
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
        EXPECT_CALL(mock_get_virtual_disk_information, Call)
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
            .WillOnce(Return(ERROR_INVALID_PARAMETER))
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
                    ASSERT_EQ(GET_VIRTUAL_DISK_INFO_SMALLEST_SAFE_VIRTUAL_SIZE, VirtualDiskInfo->Version);
                    VirtualDiskInfo->SmallestSafeVirtualSize = 123456;
                },
                Return(ERROR_SUCCESS)))
            .WillOnce(Return(ERROR_INVALID_PARAMETER));

        EXPECT_CALL(mock_close_handle, Call(Eq(mock_handle_object))).WillOnce(Return(true));
        logger_scope.mock_logger->expect_log(mpl::Level::debug, "VirtDiskWrapper::VirtDiskWrapper(...)");
        logger_scope.mock_logger->expect_log(mpl::Level::debug, "get_virtual_disk_info(...) > vhdx_path: test.vhdx");
        logger_scope.mock_logger->expect_log(mpl::Level::debug, "open_virtual_disk(...) > vhdx_path: test.vhdx");
        logger_scope.mock_logger->expect_log(mpl::Level::warning, "get_virtual_disk_info(...) > failed to get 6");
        logger_scope.mock_logger->expect_log(mpl::Level::warning, "get_virtual_disk_info(...) > failed to get 7");
    }

    {
        uut_t uut{mock_api_table};
        hyperv::virtdisk::VirtualDiskInfo info{};
        const auto& [status, status_msg] = uut.get_virtual_disk_info("test.vhdx", info);
        ASSERT_TRUE(status);
        ASSERT_TRUE(status_msg.empty());

        ASSERT_TRUE(info.size.has_value());
        ASSERT_FALSE(info.virtual_storage_type.has_value());
        ASSERT_TRUE(info.smallest_safe_virtual_size.has_value());
        ASSERT_FALSE(info.provider_subtype.has_value());

        ASSERT_EQ(info.size->virtual_, 1111111);
        ASSERT_EQ(info.size->block, 2222222);
        ASSERT_EQ(info.size->physical, 3333333);
        ASSERT_EQ(info.size->sector, 4444444);

        ASSERT_EQ(info.smallest_safe_virtual_size.value(), 123456);
    }
}

} // namespace multipass::test
