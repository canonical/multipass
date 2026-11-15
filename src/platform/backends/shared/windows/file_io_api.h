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

#include <multipass/singleton.h>

#include <windows.h>

#define MP_FILEIOAPI multipass::FileIOAPI::instance()

namespace multipass
{
/**
 * Mockable function table for the Win32 file API
 */
struct FileIOAPI : public Singleton<FileIOAPI>
{
    FileIOAPI(const Singleton<FileIOAPI>::PrivatePass&) noexcept;

    [[nodiscard]] virtual DWORD GetFileAttributesW(LPCWSTR lpFileName) const;
    [[nodiscard]] virtual HANDLE CreateFileW(LPCWSTR lpFileName,
                                             DWORD dwDesiredAccess,
                                             DWORD dwShareMode,
                                             LPSECURITY_ATTRIBUTES lpSecurityAttributes,
                                             DWORD dwCreationDisposition,
                                             DWORD dwFlagsAndAttributes,
                                             HANDLE hTemplateFile) const;
    [[nodiscard]] virtual BOOL DeviceIoControl(HANDLE hDevice,
                                               DWORD dwIoControlCode,
                                               LPVOID lpInBuffer,
                                               DWORD nInBufferSize,
                                               LPVOID lpOutBuffer,
                                               DWORD nOutBufferSize,
                                               LPDWORD lpBytesReturned,
                                               LPOVERLAPPED lpOverlapped) const;
    [[nodiscard]] virtual BOOL CloseHandle(HANDLE hObject) const;
    [[nodiscard]] virtual DWORD GetLastError() const;
};
} // namespace multipass
