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

#include <hyperv_api/virtdisk/virtdisk_wrapper.h>

#include <hyperv_api/virtdisk/virtdisk_api_table.h>

// clang-format off
#include <windows.h>
#include <initguid.h>
#include <virtdisk.h>
#include <strsafe.h>
// clang-format on

#include <fmt/std.h>
#include <fmt/xchar.h>

#include <multipass/exceptions/formatted_exception_base.h>
#include <multipass/file_ops.h>
#include <multipass/logging/log.h>

namespace multipass::hyperv::virtdisk
{

namespace
{

inline const VirtDiskAPI& API()
{
    return VirtDiskAPI::instance();
}

// helper type for the visitor #4
template <class... Ts>
struct overloaded : Ts...
{
    using Ts::operator()...;
};

auto normalize_path(std::filesystem::path p)
{
    p.make_preferred();
    return p;
}

struct HandleCloser
{
    void operator()(HANDLE h) const noexcept
    {
        (void)API().CloseHandle(h);
    }
};

using UniqueHandle = std::unique_ptr<std::remove_pointer_t<HANDLE>, HandleCloser>;

namespace mpl = logging;
using lvl = mpl::Level;

struct VirtDiskCreateError : FormattedExceptionBase<>
{
    using FormattedExceptionBase<>::FormattedExceptionBase;
};

constexpr auto log_category = "HyperV-VirtDisk-Wrapper";

UniqueHandle open_virtual_disk(
    const std::filesystem::path& vhdx_path,
    VIRTUAL_DISK_ACCESS_MASK access_mask = VIRTUAL_DISK_ACCESS_MASK::VIRTUAL_DISK_ACCESS_ALL,
    OPEN_VIRTUAL_DISK_FLAG flags = OPEN_VIRTUAL_DISK_FLAG::OPEN_VIRTUAL_DISK_FLAG_NONE,
    POPEN_VIRTUAL_DISK_PARAMETERS params = nullptr)
{
    mpl::debug(log_category, "open_virtual_disk(...) > vhdx_path: {}", vhdx_path);
    //
    // Specify UNKNOWN for both device and vendor so the system will use the
    // file extension to determine the correct VHD format.
    //
    VIRTUAL_STORAGE_TYPE type{};
    type.DeviceId = VIRTUAL_STORAGE_TYPE_DEVICE_UNKNOWN;
    type.VendorId = VIRTUAL_STORAGE_TYPE_VENDOR_UNKNOWN;

    HANDLE handle{nullptr};
    const auto path_w = vhdx_path.generic_wstring();

    const ResultCode result = API().OpenVirtualDisk(
        // [in] PVIRTUAL_STORAGE_TYPE VirtualStorageType
        &type,
        //  [in] PCWSTR Path
        path_w.c_str(),
        // [in] VIRTUAL_DISK_ACCESS_MASK VirtualDiskAccessMask
        access_mask,
        // [in] OPEN_VIRTUAL_DISK_FLAG Flags
        flags,
        // [in, optional] POPEN_VIRTUAL_DISK_PARAMETERS Parameters
        params,
        // [out] PHANDLE Handle
        &handle);

    if (!result)
    {
        mpl::error(log_category,
                   "open_virtual_disk(...) > OpenVirtualDisk failed with: {}",
                   static_cast<std::error_code>(result));
        return UniqueHandle{nullptr};
    }

    return UniqueHandle{handle};
}

} // namespace

// ---------------------------------------------------------

VirtDiskWrapper::VirtDiskWrapper(const Singleton<VirtDiskWrapper>::PrivatePass& pass) noexcept
    : Singleton<VirtDiskWrapper>::Singleton(pass)
{
}

// ---------------------------------------------------------

OperationResult VirtDiskWrapper::create_virtual_disk(
    const CreateVirtualDiskParameters& params) const
{
    mpl::debug(log_category, "create_virtual_disk(...) > params: {}", params);

    const auto target_path_normalized = normalize_path(params.path).generic_wstring();
    //
    // https://github.com/microsoft/Windows-classic-samples/blob/main/Samples/Hyper-V/Storage/cpp/CreateVirtualDisk.cpp
    //
    VIRTUAL_STORAGE_TYPE type{};

    //
    // Specify UNKNOWN for both device and vendor so the system will use the
    // file extension to determine the correct VHD format.
    //
    type.DeviceId = VIRTUAL_STORAGE_TYPE_DEVICE_VHDX;
    type.VendorId = VIRTUAL_STORAGE_TYPE_VENDOR_UNKNOWN;

    CREATE_VIRTUAL_DISK_PARAMETERS parameters{};
    parameters.Version = CREATE_VIRTUAL_DISK_VERSION_2;
    parameters.Version2 = {};
    parameters.Version2.MaximumSize = params.size_in_bytes;
    parameters.Version2.SourcePath = nullptr;
    parameters.Version2.ParentPath = nullptr;
    parameters.Version2.BlockSizeInBytes = CREATE_VIRTUAL_DISK_PARAMETERS_DEFAULT_BLOCK_SIZE;
    parameters.Version2.SectorSizeInBytes = CREATE_VIRTUAL_DISK_PARAMETERS_DEFAULT_SECTOR_SIZE;

    CREATE_VIRTUAL_DISK_FLAG flags{CREATE_VIRTUAL_DISK_FLAG_NONE};

    /**
     * The source/parent paths need to be normalized first,
     * and the normalized path needs to outlive the API call itself.
     */
    std::wstring predecessor_path_normalized{};

    auto fill_target = [this](const std::wstring& predecessor_path,
                              PCWSTR& target_path,
                              VIRTUAL_STORAGE_TYPE& target_type) {
        std::filesystem::path pp{predecessor_path};
        if (!MP_FILEOPS.exists(pp))
        {
            throw VirtDiskCreateError{"Predecessor VHDX file `{}` does not exist!", pp};
        }
        target_path = predecessor_path.c_str();
        VirtualDiskInfo predecessor_disk_info{};
        const auto result = get_virtual_disk_info(predecessor_path, predecessor_disk_info);
        mpl::debug(log_category,
                   "create_virtual_disk(...) > source disk info fetch result `{}`",
                   result);
        target_type.DeviceId = VIRTUAL_STORAGE_TYPE_DEVICE_VHDX;
        if (predecessor_disk_info.virtual_storage_type)
        {
            if (predecessor_disk_info.virtual_storage_type == "vhd")
            {
                target_type.DeviceId = VIRTUAL_STORAGE_TYPE_DEVICE_VHD;
            }
            else if (predecessor_disk_info.virtual_storage_type == "vhdx")
            {
                target_type.DeviceId = VIRTUAL_STORAGE_TYPE_DEVICE_VHDX;
            }
            else if (predecessor_disk_info.virtual_storage_type == "unknown")
            {
                throw VirtDiskCreateError{"Unable to determine predecessor disk's (`{}`) type!",
                                          pp};
            }
            else
            {
                throw VirtDiskCreateError{"Unsupported predecessor disk type"};
            }
        }
        else
        {
            throw VirtDiskCreateError{
                "Failed to retrieve the predecessor disk type for `{}`, error code: {}",
                pp,
                result};
        }
    };

    std::visit(overloaded{
                   [&](const std::monostate&) {
                       //
                       // If there's no source or parent:
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
                   },
                   [&](const SourcePathParameters& params) {
                       predecessor_path_normalized = normalize_path(params.path).wstring();
                       fill_target(predecessor_path_normalized,
                                   parameters.Version2.SourcePath,
                                   parameters.Version2.SourceVirtualStorageType);
                       flags |= CREATE_VIRTUAL_DISK_FLAG_PREVENT_WRITES_TO_SOURCE_DISK;
                       mpl::debug(log_category,
                                  "create_virtual_disk(...) > cloning `{}` to `{}`",
                                  std::filesystem::path{predecessor_path_normalized},
                                  std::filesystem::path{target_path_normalized});
                   },
                   [&](const ParentPathParameters& params) {
                       predecessor_path_normalized = normalize_path(params.path).wstring();
                       fill_target(predecessor_path_normalized,
                                   parameters.Version2.ParentPath,
                                   parameters.Version2.ParentVirtualStorageType);
                       flags |= CREATE_VIRTUAL_DISK_FLAG_PREVENT_WRITES_TO_SOURCE_DISK;
                       parameters.Version2.ParentVirtualStorageType.DeviceId =
                           VIRTUAL_STORAGE_TYPE_DEVICE_VHDX;
                       // Use parent's size.
                       parameters.Version2.MaximumSize = 0;
                   },
               },
               params.predecessor);

    HANDLE result_handle{nullptr};

    const auto result =
        API().CreateVirtualDisk(&type,
                                // [in] PCWSTR Path
                                target_path_normalized.c_str(),
                                // [in] VIRTUAL_DISK_ACCESS_MASK VirtualDiskAccessMask,
                                VIRTUAL_DISK_ACCESS_NONE,
                                // [in, optional] PSECURITY_DESCRIPTOR SecurityDescriptor,
                                nullptr,
                                // [in] CREATE_VIRTUAL_DISK_FLAG Flags,
                                flags,
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
        [[maybe_unused]] UniqueHandle _{result_handle};
        return OperationResult{NOERROR, L""};
    }

    mpl::error(log_category,
               "create_virtual_disk(...) > CreateVirtualDisk failed with {}!",
               result);
    return OperationResult{E_FAIL, fmt::format(L"CreateVirtualDisk failed with {}!", result)};
}

// ---------------------------------------------------------

OperationResult VirtDiskWrapper::resize_virtual_disk(const std::filesystem::path& vhdx_path,
                                                     std::uint64_t new_size_bytes) const
{
    mpl::debug(log_category,
               "resize_virtual_disk(...) > vhdx_path: {}, new_size_bytes: {}",
               vhdx_path,
               new_size_bytes);
    const auto disk_handle = open_virtual_disk(vhdx_path);

    if (nullptr == disk_handle)
    {
        return OperationResult{E_FAIL, L"open_virtual_disk failed!"};
    }

    RESIZE_VIRTUAL_DISK_PARAMETERS params{};
    params.Version = RESIZE_VIRTUAL_DISK_VERSION_1;
    params.Version1 = {};
    params.Version1.NewSize = new_size_bytes;

    const auto resize_result = API().ResizeVirtualDisk(
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

    mpl::error(log_category,
               "resize_virtual_disk(...) > ResizeVirtualDisk failed with {}!",
               resize_result);

    return OperationResult{E_FAIL,
                           fmt::format(L"ResizeVirtualDisk failed with {}!", resize_result)};
}

// ---------------------------------------------------------

OperationResult VirtDiskWrapper::merge_virtual_disk_to_parent(
    const std::filesystem::path& child) const
{
    // https://github.com/microsoftarchive/msdn-code-gallery-microsoft/blob/master/OneCodeTeam/Demo%20various%20VHD%20API%20usage%20(CppVhdAPI)/%5BC%2B%2B%5D-Demo%20various%20VHD%20API%20usage%20(CppVhdAPI)/C%2B%2B/CppVhdAPI/CppVhdAPI().cpp
    mpl::debug(log_category, "merge_virtual_disk_to_parent(...) > child: {}", child);

    OPEN_VIRTUAL_DISK_PARAMETERS open_params{};
    open_params.Version = OPEN_VIRTUAL_DISK_VERSION_1;
    open_params.Version1.RWDepth = 2;

    const auto child_handle =
        open_virtual_disk(child,
                          VIRTUAL_DISK_ACCESS_METAOPS | VIRTUAL_DISK_ACCESS_GET_INFO,
                          OPEN_VIRTUAL_DISK_FLAG_NONE,
                          &open_params);

    if (nullptr == child_handle)
    {
        return OperationResult{E_FAIL, L"open_virtual_disk failed!"};
    }
    MERGE_VIRTUAL_DISK_PARAMETERS params{};
    params.Version = MERGE_VIRTUAL_DISK_VERSION_1;
    params.Version1.MergeDepth = MERGE_VIRTUAL_DISK_DEFAULT_MERGE_DEPTH;

    if (const auto r = API().MergeVirtualDisk(child_handle.get(),
                                              MERGE_VIRTUAL_DISK_FLAG_NONE,
                                              &params,
                                              nullptr);
        r == ERROR_SUCCESS)
        return OperationResult{NOERROR, L""};
    else
    {
        std::error_code ec{static_cast<int>(r), std::system_category()};
        mpl::error(log_category,
                   "merge_virtual_disk_to_parent(...) > MergeVirtualDisk failed with {}!",
                   ec.message());
        return OperationResult{E_FAIL, fmt::format(L"MergeVirtualDisk failed with {}!", r)};
    }
}

// ---------------------------------------------------------

OperationResult VirtDiskWrapper::reparent_virtual_disk(const std::filesystem::path& child,
                                                       const std::filesystem::path& parent) const
{
    mpl::debug(log_category,
               "reparent_virtual_disk(...) > child: {}, new parent: {}",
               child,
               parent);

    OPEN_VIRTUAL_DISK_PARAMETERS open_parameters{};
    open_parameters.Version = OPEN_VIRTUAL_DISK_VERSION_2;
    open_parameters.Version2.GetInfoOnly = false;

    const auto child_handle = open_virtual_disk(child,
                                                VIRTUAL_DISK_ACCESS_NONE,
                                                OPEN_VIRTUAL_DISK_FLAG_NO_PARENTS,
                                                &open_parameters);

    if (nullptr == child_handle)
    {
        return OperationResult{E_FAIL, L"open_virtual_disk failed!"};
    }

    const auto parent_path_wstr = parent.generic_wstring();

    SET_VIRTUAL_DISK_INFO info{};
    // Confusing naming. version field is basically a "request type" field
    // for {Get/Set}VirtualDiskInformation.
    info.Version = SET_VIRTUAL_DISK_INFO_PARENT_PATH_WITH_DEPTH;
    info.ParentPathWithDepthInfo.ParentFilePath = parent_path_wstr.c_str();
    info.ParentPathWithDepthInfo.ChildDepth = 1; // immediate child

    if (const auto r = API().SetVirtualDiskInformation(child_handle.get(), &info);
        r == ERROR_SUCCESS)
        return OperationResult{NOERROR, L""};
    else
    {
        mpl::error(log_category,
                   "reparent_virtual_disk(...) > SetVirtualDiskInformation failed with {}!",
                   r);
        return OperationResult{E_FAIL, fmt::format(L"reparent_virtual_disk failed with {}!", r)};
    }
}

// ---------------------------------------------------------

OperationResult VirtDiskWrapper::get_virtual_disk_info(const std::filesystem::path& vhdx_path,
                                                       VirtualDiskInfo& vdinfo) const
{
    mpl::debug(log_category, "get_virtual_disk_info(...) > vhdx_path: {}", vhdx_path);
    //
    // https://github.com/microsoft/Windows-classic-samples/blob/main/Samples/Hyper-V/Storage/cpp/GetVirtualDiskInformation.cpp
    //

    const auto disk_handle = open_virtual_disk(vhdx_path);

    if (nullptr == disk_handle)
    {
        return OperationResult{E_FAIL, L"open_virtual_disk failed!"};
    }

    constexpr GET_VIRTUAL_DISK_INFO_VERSION what_to_get[] = {
        GET_VIRTUAL_DISK_INFO_SIZE,
        GET_VIRTUAL_DISK_INFO_VIRTUAL_STORAGE_TYPE,
        GET_VIRTUAL_DISK_INFO_PROVIDER_SUBTYPE};

    for (const auto version : what_to_get)
    {
        GET_VIRTUAL_DISK_INFO disk_info{};
        disk_info.Version = version;

        ULONG sz = sizeof(disk_info);

        const auto result =
            API().GetVirtualDiskInformation(disk_handle.get(), &sz, &disk_info, nullptr);

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
            mpl::warn(log_category,
                      "get_virtual_disk_info(...) > failed to get {}",
                      fmt::underlying(version));
        }
    }

    return {NOERROR, L""};
}

OperationResult VirtDiskWrapper::list_virtual_disk_chain(const std::filesystem::path& vhdx_path,
                                                         std::vector<std::filesystem::path>& chain,
                                                         std::optional<std::size_t> max_depth) const
{

    mpl::debug(log_category, "list_virtual_disk_chain(...) > vhdx_path: {}", vhdx_path);
    // https://github.com/microsoft/Windows-classic-samples/blob/main/Samples/Hyper-V/Storage/cpp/GetVirtualDiskInformation.cpp#L285

    // Check if given vhdx is a differencing disk.
    std::filesystem::path current = vhdx_path;

    auto alloc = [](const std::size_t sz = sizeof(GET_VIRTUAL_DISK_INFO)) {
        return std::unique_ptr<GET_VIRTUAL_DISK_INFO, decltype(&std::free)>(
            static_cast<GET_VIRTUAL_DISK_INFO*>(std::malloc(sz)),
            std::free);
    };

    do
    {
        // Heap-alloc since we're going to re-allocate it for the trailing
        // variable length array.
        auto disk_info = alloc();
        disk_info->Version = GET_VIRTUAL_DISK_INFO_PARENT_LOCATION;

        const auto disk_handle = open_virtual_disk(current);

        if (nullptr == disk_handle)
            return OperationResult{E_FAIL, L"open_virtual_disk failed!"};

        chain.push_back(current);

        ULONG sz = sizeof(GET_VIRTUAL_DISK_INFO);

        // Here we are calling the GetVirtualDiskInformation function to obtain the parent disk
        // path, if any. The API stores the parent's path into a variable array field, which needs
        // to be allocated by us. Hence, the API first returns ERROR_INSUFFICIENT_BUFFER to tell us
        // how much extra space is needed for storing the parent's path.
        // If the disk does not have a parent, the API would return ERROR_VHD_INVALID_TYPE instead.
        if (const auto r =
                GetVirtualDiskInformation(disk_handle.get(), &sz, disk_info.get(), nullptr);
            r == ERROR_INSUFFICIENT_BUFFER)
        {
            // Reallocate the disk_info struct with the correct size, and also re-set
            // the version field as it's not an in-place re-allocation.
            disk_info = alloc(sz);
            disk_info->Version = GET_VIRTUAL_DISK_INFO_PARENT_LOCATION;
        }
        else if (r == ERROR_VHD_INVALID_TYPE)
        {
            // End of the chain, or not a chain at all.
            // Either way, end the loop.
            break;
        }
        else
            return OperationResult{E_FAIL, L"GetVirtualDiskInformation failed!"};

        // This is the real call to obtain the parent path.
        const auto r = GetVirtualDiskInformation(disk_handle.get(), &sz, disk_info.get(), nullptr);

        if (r == ERROR_SUCCESS)
        {
            // The ParentLocationBuffer field is multi-purposed. It might contain a single string,
            // or multiple strings, each denoting a possible path for the VHDX file.
            if (disk_info->ParentLocation.ParentResolved)
            {
                // Single string.
                const auto parent_buffer_size =
                    sz - FIELD_OFFSET(GET_VIRTUAL_DISK_INFO, ParentLocation.ParentLocationBuffer);
                std::size_t parent_path_size = {0};
                if (FAILED(StringCbLengthW(disk_info->ParentLocation.ParentLocationBuffer,
                                           parent_buffer_size,
                                           &parent_path_size)))
                {
                    return OperationResult{E_FAIL, L"StringCbLengthW failed!"};
                }
                current = std::wstring{disk_info->ParentLocation.ParentLocationBuffer,
                                       parent_path_size / sizeof(wchar_t)};
                continue;
            }
            else
                // If ParentResolved is faise, that means ParentLocationBuffer contains multiple
                // strings. The use-case for it is recording multiple possible paths for a disk.
                // Hyper-V uses this feature to resolve moved disks, which is not typical for our
                // use-case.
                return OperationResult{E_FAIL, L"Parent virtual disk path resolution failed!"};
        }
    } while (!max_depth || --(*max_depth));
    mpl::debug(log_category,
               "list_virtual_disk_chain(...) > final chain: {}",
               fmt::join(chain, " | --> | "));
    return {NOERROR, L""};
}

} // namespace multipass::hyperv::virtdisk
