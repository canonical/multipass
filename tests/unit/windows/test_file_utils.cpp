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

#include "tests/unit/common.h"
#include "tests/unit/mock_logger.h"
#include "tests/unit/windows/mock_file_io_api.h"

#include "shared/windows/file_utils.h"

#include <winioctl.h>

namespace mp = multipass;
namespace mpl = multipass::logging;
namespace mpt = multipass::test;

using namespace testing;

namespace
{
struct WindowsFileUtils : public Test
{
    mpt::MockLogger::Scope logger_scope = [] {
        auto scope = mpt::MockLogger::inject();
        scope.mock_logger->screen_logs(mpl::Level::info);
        return scope;
    }();

    mpt::MockFileIOAPI::GuardedMock mock_file_io_api_injection =
        mpt::MockFileIOAPI::inject<StrictMock>();
    mpt::MockFileIOAPI& mock_file_io_api = *mock_file_io_api_injection.first;

    // Opaque sentinel handle, only compared by value.
    inline static const auto mock_handle = reinterpret_cast<HANDLE>(0xbadf00d);
    inline static const std::filesystem::path path{L"test.vhdx"};
};

TEST_F(WindowsFileUtils, clearSparseFlagClearsSparseFile)
{
    EXPECT_CALL(mock_file_io_api, GetFileAttributesW(StrEq(L"test.vhdx")))
        .WillOnce(Return(FILE_ATTRIBUTE_ARCHIVE | FILE_ATTRIBUTE_SPARSE_FILE));
    EXPECT_CALL(mock_file_io_api,
                CreateFileW(StrEq(L"test.vhdx"), // lpFileName
                            GENERIC_WRITE,       // dwDesiredAccess
                            FILE_SHARE_READ,     // dwShareMode
                            IsNull(),            // lpSecurityAttributes
                            OPEN_EXISTING,       // dwCreationDisposition
                            0,                   // dwFlagsAndAttributes
                            IsNull()             // hTemplateFile
                            ))
        .WillOnce(Return(mock_handle));
    EXPECT_CALL(mock_file_io_api,
                DeviceIoControl(
                    mock_handle,      // hDevice
                    FSCTL_SET_SPARSE, // dwIoControlCode
                    AllOf(NotNull(), Truly([](const void* buffer) {
                              return !static_cast<const FILE_SET_SPARSE_BUFFER*>(buffer)->SetSparse;
                          })),                      // lpInBuffer
                    sizeof(FILE_SET_SPARSE_BUFFER), // nInBufferSize
                    IsNull(),                       // lpOutBuffer
                    0,                              // nOutBufferSize
                    NotNull(),                      // lpBytesReturned
                    IsNull()                        // lpOverlapped
                    ))
        .WillOnce(Return(TRUE));
    EXPECT_CALL(mock_file_io_api, CloseHandle(mock_handle)).WillOnce(Return(TRUE));

    EXPECT_FALSE(mp::clear_sparse_flag(path));
}

TEST_F(WindowsFileUtils, clearSparseFlagLeavesNonSparseFileUntouched)
{
    EXPECT_CALL(mock_file_io_api, GetFileAttributesW(StrEq(L"test.vhdx")))
        .WillOnce(Return(FILE_ATTRIBUTE_ARCHIVE));

    EXPECT_FALSE(mp::clear_sparse_flag(path));
}

TEST_F(WindowsFileUtils, clearSparseFlagFailsWhenAttributesUnavailable)
{
    EXPECT_CALL(mock_file_io_api, GetFileAttributesW).WillOnce(Return(INVALID_FILE_ATTRIBUTES));
    EXPECT_CALL(mock_file_io_api, GetLastError).WillOnce(Return(ERROR_FILE_NOT_FOUND));
    logger_scope.mock_logger->expect_log(mpl::Level::error, "GetFileAttributesW failed");

    const auto ec = mp::clear_sparse_flag(path);
    EXPECT_EQ(ec.value(), ERROR_FILE_NOT_FOUND);
    EXPECT_EQ(ec.category(), std::system_category());
}

TEST_F(WindowsFileUtils, clearSparseFlagFailsWhenFileCannotBeOpened)
{
    EXPECT_CALL(mock_file_io_api, GetFileAttributesW).WillOnce(Return(FILE_ATTRIBUTE_SPARSE_FILE));
    EXPECT_CALL(mock_file_io_api, CreateFileW).WillOnce(Return(INVALID_HANDLE_VALUE));
    EXPECT_CALL(mock_file_io_api, GetLastError).WillOnce(Return(ERROR_ACCESS_DENIED));
    logger_scope.mock_logger->expect_log(mpl::Level::error, "CreateFileW failed");

    EXPECT_EQ(mp::clear_sparse_flag(path).value(), ERROR_ACCESS_DENIED);
}

TEST_F(WindowsFileUtils, clearSparseFlagFailsWhenFlagCannotBeCleared)
{
    EXPECT_CALL(mock_file_io_api, GetFileAttributesW).WillOnce(Return(FILE_ATTRIBUTE_SPARSE_FILE));
    EXPECT_CALL(mock_file_io_api, CreateFileW).WillOnce(Return(mock_handle));
    logger_scope.mock_logger->expect_log(mpl::Level::error,
                                         "DeviceIoControl(FSCTL_SET_SPARSE) failed");

    // The error code must be read before the handle is closed.
    InSequence sequence;
    EXPECT_CALL(mock_file_io_api, DeviceIoControl).WillOnce(Return(FALSE));
    EXPECT_CALL(mock_file_io_api, GetLastError).WillOnce(Return(ERROR_INVALID_FUNCTION));
    EXPECT_CALL(mock_file_io_api, CloseHandle(mock_handle)).WillOnce(Return(TRUE));

    EXPECT_EQ(mp::clear_sparse_flag(path).value(), ERROR_INVALID_FUNCTION);
}
} // namespace
