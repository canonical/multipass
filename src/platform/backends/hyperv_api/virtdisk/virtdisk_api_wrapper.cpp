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

#include <hyperv_api/virtdisk/virtdisk_api_wrapper.h>

#include <initguid.h>
#include <virtdisk.h>
#include <windows.h>


#include <multipass/logging/log.h>
#include <fmt/xchar.h>

namespace multipass::hyperv::virtdisk
{

namespace
{

using UniqueHandle = std::unique_ptr<std::remove_pointer_t<HANDLE>, decltype(VirtDiskAPITable::CloseHandle)>;

namespace mpl = logging;
using lvl = mpl::Level;

/**
 * Category for the log messages.
 */
constexpr auto kLogCategory = "HyperV-VirtDisk-Wrapper";

UniqueHandle open_virtual_disk(const VirtDiskAPITable& api, const std::filesystem::path& vhdx_path)
{
    mpl::debug(kLogCategory, "open_virtual_disk(...) > vhdx_path: {}", vhdx_path.string());
    //
    // Specify UNKNOWN for both device and vendor so the system will use the
    // file extension to determine the correct VHD format.
    //
    VIRTUAL_STORAGE_TYPE type{};
    type.DeviceId = VIRTUAL_STORAGE_TYPE_DEVICE_UNKNOWN;
    type.VendorId = VIRTUAL_STORAGE_TYPE_VENDOR_UNKNOWN;

    HANDLE handle{nullptr};
    const auto path_w = vhdx_path.wstring();

    const auto result = api.OpenVirtualDisk(
        // [in] PVIRTUAL_STORAGE_TYPE VirtualStorageType
        &type,
        //  [in] PCWSTR Path
        path_w.c_str(),
        // [in] VIRTUAL_DISK_ACCESS_MASK VirtualDiskAccessMask
        VIRTUAL_DISK_ACCESS_ALL,
        // [in] OPEN_VIRTUAL_DISK_FLAG Flags
        OPEN_VIRTUAL_DISK_FLAG_NONE,
        // [in, optional] POPEN_VIRTUAL_DISK_PARAMETERS Parameters
        nullptr,
        // [out] PHANDLE Handle
        &handle);

    if (!(result == ERROR_SUCCESS))
    {
        mpl::error(kLogCategory, "open_virtual_disk(...) > OpenVirtualDisk failed with: {}", result);
        return UniqueHandle{nullptr, api.CloseHandle};
    }

    return {handle, api.CloseHandle};
}

} // namespace

// ---------------------------------------------------------

VirtDiskWrapper::VirtDiskWrapper(const VirtDiskAPITable& api_table) : api{api_table}
{
    mpl::debug(kLogCategory, "VirtDiskWrapper::VirtDiskWrapper(...) > api_table: {}", api);
}

// ---------------------------------------------------------

OperationResult VirtDiskWrapper::create_virtual_disk(const CreateVirtualDiskParameters& params) const
{
    mpl::debug(kLogCategory, "create_virtual_disk(...) > params: {}", params);
    //
    // https://github.com/microsoft/Windows-classic-samples/blob/main/Samples/Hyper-V/Storage/cpp/CreateVirtualDisk.cpp
    //
    VIRTUAL_STORAGE_TYPE type{};

    //
    // Specify UNKNOWN for both device and vendor so the system will use the
    // file extension to determine the correct VHD format.
    //
    type.DeviceId = VIRTUAL_STORAGE_TYPE_DEVICE_UNKNOWN;
    type.VendorId = VIRTUAL_STORAGE_TYPE_VENDOR_UNKNOWN;

    CREATE_VIRTUAL_DISK_PARAMETERS parameters{};
    parameters.Version = CREATE_VIRTUAL_DISK_VERSION_2;
    parameters.Version2 = {};
    parameters.Version2.MaximumSize = params.size_in_bytes;

    //
    // Internal size of the virtual disk object blocks, in bytes.
    // For VHDX this must be a multiple of 1 MB between 1 and 256 MB.
    // For VHD 1 this must be set to one of the following values.
    // parameters.Version2.BlockSizeInBytes
    //
    parameters.Version2.BlockSizeInBytes = 1048576; // 1024 KiB

    if (params.path.extension() == ".vhd")
    {
        parameters.Version2.BlockSizeInBytes = 524288; // 512 KiB
    }

    const auto path_w = params.path.wstring();

    HANDLE result_handle{nullptr};

    const auto result = api.CreateVirtualDisk(&type,
                                              // [in] PCWSTR Path
                                              path_w.c_str(),
                                              // [in] VIRTUAL_DISK_ACCESS_MASK VirtualDiskAccessMask,
                                              VIRTUAL_DISK_ACCESS_NONE,
                                              // [in, optional] PSECURITY_DESCRIPTOR SecurityDescriptor,
                                              nullptr,
                                              // [in] CREATE_VIRTUAL_DISK_FLAG Flags,
                                              CREATE_VIRTUAL_DISK_FLAG_NONE,
                                              // [in] ULONG ProviderSpecificFlags,
                                              0,
                                              // [in] PCREATE_VIRTUAL_DISK_PARAMETERS Parameters,
                                              &parameters,
                                              // [in, optional] LPOVERLAPPED Overlapped
                                              nullptr,
                                              // [out] PHANDLE Handle
                                              &result_handle);

    if (result == ERROR_SUCCESS)
    {
        [[maybe_unused]] UniqueHandle _{result_handle, api.CloseHandle};
        return OperationResult{NOERROR, L""};
    }

    mpl::error(kLogCategory, "create_virtual_disk(...) > CreateVirtualDisk failed with {}!", result);
    return OperationResult{E_FAIL, fmt::format(L"CreateVirtualDisk failed with {}!", result)};
}

// ---------------------------------------------------------

OperationResult VirtDiskWrapper::resize_virtual_disk(const std::filesystem::path& vhdx_path,
                                                     std::uint64_t new_size_bytes) const
{
    mpl::debug(kLogCategory,
               "resize_virtual_disk(...) > vhdx_path: {}, new_size_bytes: {}",
               vhdx_path.string(),
               new_size_bytes);
    const auto disk_handle = open_virtual_disk(api, vhdx_path);

    if (nullptr == disk_handle)
    {
        return OperationResult{E_FAIL, L"open_virtual_disk failed!"};
    }

    RESIZE_VIRTUAL_DISK_PARAMETERS params{};
    params.Version = RESIZE_VIRTUAL_DISK_VERSION_1;
    params.Version1 = {};
    params.Version1.NewSize = new_size_bytes;

    const auto resize_result = api.ResizeVirtualDisk(
        // [in] HANDLE VirtualDiskHandle
        disk_handle.get(),
        // [in] RESIZE_VIRTUAL_DISK_FLAG Flags
        RESIZE_VIRTUAL_DISK_FLAG_NONE,
        // [in] PRESIZE_VIRTUAL_DISK_PARAMETERS Parameters
        &params,
        // [in, optional] LPOVERLAPPED Overlapped
        nullptr);

    if (ERROR_SUCCESS == resize_result)
    {
        return OperationResult{NOERROR, L""};
    }

    mpl::error(kLogCategory, "resize_virtual_disk(...) > ResizeVirtualDisk failed with {}!", resize_result);

    return OperationResult{E_FAIL, fmt::format(L"ResizeVirtualDisk failed with {}!", resize_result)};
}

// ---------------------------------------------------------

OperationResult VirtDiskWrapper::get_virtual_disk_info(const std::filesystem::path& vhdx_path,
                                                       VirtualDiskInfo& vdinfo) const
{
    mpl::debug(kLogCategory, "get_virtual_disk_info(...) > vhdx_path: {}", vhdx_path.string());
    //
    // https://github.com/microsoft/Windows-classic-samples/blob/main/Samples/Hyper-V/Storage/cpp/GetVirtualDiskInformation.cpp
    //

    const auto disk_handle = open_virtual_disk(api, vhdx_path);

    if (nullptr == disk_handle)
    {
        return OperationResult{E_FAIL, L"open_virtual_disk failed!"};
    }

    constexpr GET_VIRTUAL_DISK_INFO_VERSION what_to_get[] = {GET_VIRTUAL_DISK_INFO_SIZE,
                                                             GET_VIRTUAL_DISK_INFO_VIRTUAL_STORAGE_TYPE,
                                                             GET_VIRTUAL_DISK_INFO_SMALLEST_SAFE_VIRTUAL_SIZE,
                                                             GET_VIRTUAL_DISK_INFO_PROVIDER_SUBTYPE};

    for (const auto version : what_to_get)
    {
        GET_VIRTUAL_DISK_INFO disk_info{};
        disk_info.Version = version;

        ULONG sz = sizeof(disk_info);

        const auto result = api.GetVirtualDiskInformation(disk_handle.get(), &sz, &disk_info, nullptr);

        if (ERROR_SUCCESS == result)
        {
            switch (disk_info.Version)
            {
            case GET_VIRTUAL_DISK_INFO_SIZE:
                vdinfo.size = std::make_optional<VirtualDiskInfo::size_info>();
                vdinfo.size->virtual_ = disk_info.Size.VirtualSize;
                vdinfo.size->block = disk_info.Size.BlockSize;
                vdinfo.size->physical = disk_info.Size.PhysicalSize;
                vdinfo.size->sector = disk_info.Size.SectorSize;
                break;
            case GET_VIRTUAL_DISK_INFO_VIRTUAL_STORAGE_TYPE:
            {
                switch (disk_info.VirtualStorageType.DeviceId)
                {
                case VIRTUAL_STORAGE_TYPE_DEVICE_UNKNOWN:
                    vdinfo.virtual_storage_type = "unknown";
                    break;
                case VIRTUAL_STORAGE_TYPE_DEVICE_ISO:
                    vdinfo.virtual_storage_type = "iso";
                    break;
                case VIRTUAL_STORAGE_TYPE_DEVICE_VHD:
                    vdinfo.virtual_storage_type = "vhd";
                    break;
                case VIRTUAL_STORAGE_TYPE_DEVICE_VHDX:
                    vdinfo.virtual_storage_type = "vhdx";
                    break;
                case VIRTUAL_STORAGE_TYPE_DEVICE_VHDSET:
                    vdinfo.virtual_storage_type = "vhdset";
                    break;
                }
            }
            break;
            case GET_VIRTUAL_DISK_INFO_SMALLEST_SAFE_VIRTUAL_SIZE:
                vdinfo.smallest_safe_virtual_size = disk_info.SmallestSafeVirtualSize;
                break;
            case GET_VIRTUAL_DISK_INFO_PROVIDER_SUBTYPE:
            {
                enum class ProviderSubtype : std::uint8_t
                {
                    fixed = 2,
                    dynamic = 3,
                    differencing = 4
                };

                switch (static_cast<ProviderSubtype>(disk_info.ProviderSubtype))
                {
                case ProviderSubtype::fixed:
                    vdinfo.provider_subtype = "fixed";
                    break;
                case ProviderSubtype::dynamic:
                    vdinfo.provider_subtype = "dynamic";

                    break;
                case ProviderSubtype::differencing:
                    vdinfo.provider_subtype = "differencing";
                    break;
                default:
                    vdinfo.provider_subtype = "unknown";
                    break;
                }
            }
            break;
            default:
                assert(0);
                break;
            }
        }
        else
        {
            mpl::warn(kLogCategory, "get_virtual_disk_info(...) > failed to get {}", fmt::underlying(version));
        }
    }

    return {NOERROR, L""};
}

} // namespace multipass::hyperv::virtdisk
