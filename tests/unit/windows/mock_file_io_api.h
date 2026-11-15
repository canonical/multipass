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

#include "shared/windows/file_io_api.h"
#include "tests/unit/mock_singleton_helpers.h"

namespace multipass::test
{
class MockFileIOAPI : public FileIOAPI
{
public:
    using FileIOAPI::FileIOAPI;

    MOCK_METHOD(DWORD, GetFileAttributesW, (LPCWSTR lpFileName), (const, override));
    MOCK_METHOD(HANDLE,
                CreateFileW,
                (LPCWSTR lpFileName,
                 DWORD dwDesiredAccess,
                 DWORD dwShareMode,
                 LPSECURITY_ATTRIBUTES lpSecurityAttributes,
                 DWORD dwCreationDisposition,
                 DWORD dwFlagsAndAttributes,
                 HANDLE hTemplateFile),
                (const, override));
    MOCK_METHOD(BOOL,
                DeviceIoControl,
                (HANDLE hDevice,
                 DWORD dwIoControlCode,
                 LPVOID lpInBuffer,
                 DWORD nInBufferSize,
                 LPVOID lpOutBuffer,
                 DWORD nOutBufferSize,
                 LPDWORD lpBytesReturned,
                 LPOVERLAPPED lpOverlapped),
                (const, override));
    MOCK_METHOD(BOOL, CloseHandle, (HANDLE hObject), (const, override));
    MOCK_METHOD(DWORD, GetLastError, (), (const, override));

    MP_MOCK_SINGLETON_BOILERPLATE(MockFileIOAPI, FileIOAPI);
};
} // namespace multipass::test
