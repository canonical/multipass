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

#include <hyperv_api/virtdisk/virtdisk_api.h>

#include "../mock_singleton_helpers.h"

namespace multipass::test
{

class MockVirtDiskAPI : public hyperv::virtdisk::VirtDiskAPI
{
public:
    using VirtDiskAPI::VirtDiskAPI;
    MOCK_METHOD(DWORD,
                CreateVirtualDisk,
                (PVIRTUAL_STORAGE_TYPE VirtualStorageType,
                 PCWSTR Path,
                 VIRTUAL_DISK_ACCESS_MASK VirtualDiskAccessMask,
                 PSECURITY_DESCRIPTOR SecurityDescriptor,
                 CREATE_VIRTUAL_DISK_FLAG Flags,
                 ULONG ProviderSpecificFlags,
                 PCREATE_VIRTUAL_DISK_PARAMETERS Parameters,
                 LPOVERLAPPED Overlapped,
                 PHANDLE Handle),
                (const, override));
    MOCK_METHOD(DWORD,
                OpenVirtualDisk,
                (PVIRTUAL_STORAGE_TYPE VirtualStorageType,
                 PCWSTR Path,
                 VIRTUAL_DISK_ACCESS_MASK VirtualDiskAccessMask,
                 OPEN_VIRTUAL_DISK_FLAG Flags,
                 POPEN_VIRTUAL_DISK_PARAMETERS Parameters,
                 PHANDLE Handle),
                (const, override));
    MOCK_METHOD(DWORD,
                ResizeVirtualDisk,
                (HANDLE VirtualDiskHandle,
                 RESIZE_VIRTUAL_DISK_FLAG Flags,
                 PRESIZE_VIRTUAL_DISK_PARAMETERS Parameters,
                 LPOVERLAPPED Overlapped),
                (const, override));
    MOCK_METHOD(DWORD,
                MergeVirtualDisk,
                (HANDLE VirtualDiskHandle,
                 MERGE_VIRTUAL_DISK_FLAG Flags,
                 PMERGE_VIRTUAL_DISK_PARAMETERS Parameters,
                 LPOVERLAPPED Overlapped),
                (const, override));
    MOCK_METHOD(DWORD,
                GetVirtualDiskInformation,
                (HANDLE VirtualDiskHandle,
                 PULONG VirtualDiskInfoSize,
                 PGET_VIRTUAL_DISK_INFO VirtualDiskInfo,
                 PULONG SizeUsed),
                (const, override));
    MOCK_METHOD(DWORD,
                SetVirtualDiskInformation,
                (HANDLE VirtualDiskHandle, PSET_VIRTUAL_DISK_INFO VirtualDiskInfo),
                (const, override));
    MOCK_METHOD(BOOL, CloseHandle, (HANDLE hObject), (const, override));

    MP_MOCK_SINGLETON_BOILERPLATE(MockVirtDiskAPI, VirtDiskAPI);
};
} // namespace multipass::test
