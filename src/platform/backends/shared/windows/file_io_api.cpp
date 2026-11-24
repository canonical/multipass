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

#include "file_io_api.h"

namespace multipass
{
FileIOAPI::FileIOAPI(const Singleton<FileIOAPI>::PrivatePass& pass) noexcept
    : Singleton<FileIOAPI>::Singleton{pass}
{
}

DWORD FileIOAPI::GetFileAttributesW(LPCWSTR lpFileName) const
{
    return ::GetFileAttributesW(lpFileName);
}

HANDLE FileIOAPI::CreateFileW(LPCWSTR lpFileName,
                              DWORD dwDesiredAccess,
                              DWORD dwShareMode,
                              LPSECURITY_ATTRIBUTES lpSecurityAttributes,
                              DWORD dwCreationDisposition,
                              DWORD dwFlagsAndAttributes,
                              HANDLE hTemplateFile) const
{
    return ::CreateFileW(lpFileName,
                         dwDesiredAccess,
                         dwShareMode,
                         lpSecurityAttributes,
                         dwCreationDisposition,
                         dwFlagsAndAttributes,
                         hTemplateFile);
}

BOOL FileIOAPI::DeviceIoControl(HANDLE hDevice,
                                DWORD dwIoControlCode,
                                LPVOID lpInBuffer,
                                DWORD nInBufferSize,
                                LPVOID lpOutBuffer,
                                DWORD nOutBufferSize,
                                LPDWORD lpBytesReturned,
                                LPOVERLAPPED lpOverlapped) const
{
    return ::DeviceIoControl(hDevice,
                             dwIoControlCode,
                             lpInBuffer,
                             nInBufferSize,
                             lpOutBuffer,
                             nOutBufferSize,
                             lpBytesReturned,
                             lpOverlapped);
}

BOOL FileIOAPI::CloseHandle(HANDLE hObject) const
{
    return ::CloseHandle(hObject);
}

DWORD FileIOAPI::GetLastError() const
{
    return ::GetLastError();
}
} // namespace multipass
