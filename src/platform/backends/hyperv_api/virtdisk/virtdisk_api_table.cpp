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

#include <hyperv_api/virtdisk/virtdisk_api_table.h>

namespace multipass::hyperv::virtdisk
{

VirtDiskAPI::VirtDiskAPI(const Singleton<VirtDiskAPI>::PrivatePass& pass) noexcept
    : Singleton<VirtDiskAPI>(pass)
{
}

DWORD VirtDiskAPI::CreateVirtualDisk(PVIRTUAL_STORAGE_TYPE VirtualStorageType,
                                     PCWSTR Path,
                                     VIRTUAL_DISK_ACCESS_MASK VirtualDiskAccessMask,
                                     PSECURITY_DESCRIPTOR SecurityDescriptor,
                                     CREATE_VIRTUAL_DISK_FLAG Flags,
                                     ULONG ProviderSpecificFlags,
                                     PCREATE_VIRTUAL_DISK_PARAMETERS Parameters,
                                     LPOVERLAPPED Overlapped,
                                     PHANDLE Handle) const
{
    return ::CreateVirtualDisk(VirtualStorageType,
                               Path,
                               VirtualDiskAccessMask,
                               SecurityDescriptor,
                               Flags,
                               ProviderSpecificFlags,
                               Parameters,
                               Overlapped,
                               Handle);
}
DWORD VirtDiskAPI::OpenVirtualDisk(PVIRTUAL_STORAGE_TYPE VirtualStorageType,
                                   PCWSTR Path,
                                   VIRTUAL_DISK_ACCESS_MASK VirtualDiskAccessMask,
                                   OPEN_VIRTUAL_DISK_FLAG Flags,
                                   POPEN_VIRTUAL_DISK_PARAMETERS Parameters,
                                   PHANDLE Handle) const
{
    return ::OpenVirtualDisk(VirtualStorageType,
                             Path,
                             VirtualDiskAccessMask,
                             Flags,
                             Parameters,
                             Handle);
}
DWORD VirtDiskAPI::ResizeVirtualDisk(HANDLE VirtualDiskHandle,
                                     RESIZE_VIRTUAL_DISK_FLAG Flags,
                                     PRESIZE_VIRTUAL_DISK_PARAMETERS Parameters,
                                     LPOVERLAPPED Overlapped) const
{
    return ::ResizeVirtualDisk(VirtualDiskHandle, Flags, Parameters, Overlapped);
}
DWORD VirtDiskAPI::MergeVirtualDisk(HANDLE VirtualDiskHandle,
                                    MERGE_VIRTUAL_DISK_FLAG Flags,
                                    PMERGE_VIRTUAL_DISK_PARAMETERS Parameters,
                                    LPOVERLAPPED Overlapped) const
{
    return ::MergeVirtualDisk(VirtualDiskHandle, Flags, Parameters, Overlapped);
}
DWORD VirtDiskAPI::GetVirtualDiskInformation(HANDLE VirtualDiskHandle,
                                             PULONG VirtualDiskInfoSize,
                                             PGET_VIRTUAL_DISK_INFO VirtualDiskInfo,
                                             PULONG SizeUsed) const
{
    return ::GetVirtualDiskInformation(VirtualDiskHandle,
                                       VirtualDiskInfoSize,
                                       VirtualDiskInfo,
                                       SizeUsed);
}
DWORD VirtDiskAPI::SetVirtualDiskInformation(HANDLE VirtualDiskHandle,
                                             PSET_VIRTUAL_DISK_INFO VirtualDiskInfo) const
{
    return ::SetVirtualDiskInformation(VirtualDiskHandle, VirtualDiskInfo);
}
BOOL VirtDiskAPI::CloseHandle(HANDLE hObject) const
{
    return ::CloseHandle(hObject);
}

} // namespace multipass::hyperv::virtdisk
