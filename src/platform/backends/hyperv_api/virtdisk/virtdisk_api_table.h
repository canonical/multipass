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

// clang-format off
#include <windows.h>
#include <initguid.h>
#include <Virtdisk.h>
// clang-format on

#include <functional>

#include <fmt/format.h>

namespace multipass::hyperv::virtdisk
{

/**
 * API function table for the virtdisk API
 * @ref https://learn.microsoft.com/en-us/windows/win32/api/virtdisk/
 */
struct VirtDiskAPITable
{
    // @ref
    // https://learn.microsoft.com/en-us/windows/win32/api/virtdisk/nf-virtdisk-createvirtualdisk
    std::function<decltype(::CreateVirtualDisk)> CreateVirtualDisk = &::CreateVirtualDisk;
    // @ref https://learn.microsoft.com/en-us/windows/win32/api/virtdisk/nf-virtdisk-openvirtualdisk
    std::function<decltype(::OpenVirtualDisk)> OpenVirtualDisk = &::OpenVirtualDisk;
    // @ref
    // https://learn.microsoft.com/en-us/windows/win32/api/virtdisk/nf-virtdisk-resizevirtualdisk
    std::function<decltype(::ResizeVirtualDisk)> ResizeVirtualDisk = &::ResizeVirtualDisk;
    // @ref
    // https://learn.microsoft.com/en-us/windows/win32/api/virtdisk/nf-virtdisk-mergevirtualdisk
    std::function<decltype(::MergeVirtualDisk)> MergeVirtualDisk = &::MergeVirtualDisk;
    // @ref
    // https://learn.microsoft.com/en-us/windows/win32/api/virtdisk/nf-virtdisk-getvirtualdiskinformation
    std::function<decltype(::GetVirtualDiskInformation)> GetVirtualDiskInformation =
        &::GetVirtualDiskInformation;
    // @ref
    // https://learn.microsoft.com/en-us/windows/win32/api/virtdisk/nf-virtdisk-setvirtualdiskinformation
    std::function<decltype(::SetVirtualDiskInformation)> SetVirtualDiskInformation =
        &::SetVirtualDiskInformation;
    // @ref https://learn.microsoft.com/en-us/windows/win32/api/handleapi/nf-handleapi-closehandle
    std::function<decltype(::CloseHandle)> CloseHandle = &::CloseHandle;
};

} // namespace multipass::hyperv::virtdisk

/**
 * Formatter type specialization for VirtDiskAPITable
 */
template <typename Char>
struct fmt::formatter<multipass::hyperv::virtdisk::VirtDiskAPITable, Char>
{
    constexpr auto parse(basic_format_parse_context<Char>& ctx)
    {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const multipass::hyperv::virtdisk::VirtDiskAPITable& api, FormatContext& ctx) const
    {
        return fmt::format_to(
            ctx.out(),
            "CreateVirtualDisk: ({}) | OpenVirtualDisk ({}) | ResizeVirtualDisk: ({}) | "
            "MergeVirtualDisk: ({}) | GetVirtualDiskInformation: ({}) | SetVirtualDiskInformation: "
            "({}) | CloseHandle: ({})",
            static_cast<bool>(api.CreateVirtualDisk),
            static_cast<bool>(api.OpenVirtualDisk),
            static_cast<bool>(api.ResizeVirtualDisk),
            static_cast<bool>(api.MergeVirtualDisk),
            static_cast<bool>(api.GetVirtualDiskInformation),
            static_cast<bool>(api.SetVirtualDiskInformation),
            static_cast<bool>(api.CloseHandle));
    }
};
