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

#ifndef MULTIPASS_HYPERV_API_HCS_API_TABLE
#define MULTIPASS_HYPERV_API_HCS_API_TABLE

// clang-format off
// (xmkg): clang-format is messing with the include order.
#include <windows.h>
#include <computecore.h>
// clang-format on

#include <functional>

#include <fmt/format.h>

namespace multipass::hyperv::hcs
{

/**
 * API function table for host compute system
 * @ref https://learn.microsoft.com/en-us/virtualization/api/hcs/reference/apioverview
 */
struct HCSAPITable
{
    // @ref https://learn.microsoft.com/en-us/virtualization/api/hcs/reference/hcscreateoperation
    std::function<decltype(HcsCreateOperation)> CreateOperation = &HcsCreateOperation;
    // @ref https://learn.microsoft.com/en-us/virtualization/api/hcs/reference/hcswaitforoperationresult
    std::function<decltype(HcsWaitForOperationResult)> WaitForOperationResult = &HcsWaitForOperationResult;
    // @ref https://learn.microsoft.com/en-us/virtualization/api/hcs/reference/hcscloseoperation
    std::function<decltype(HcsCloseOperation)> CloseOperation = &HcsCloseOperation;
    // @ref https://learn.microsoft.com/en-us/virtualization/api/hcs/reference/hcscreatecomputesystem
    std::function<decltype(HcsCreateComputeSystem)> CreateComputeSystem = &HcsCreateComputeSystem;
    // @ref https://learn.microsoft.com/en-us/virtualization/api/hcs/reference/hcsopencomputesystem
    std::function<decltype(HcsOpenComputeSystem)> OpenComputeSystem = &HcsOpenComputeSystem;
    // @ref https://learn.microsoft.com/en-us/virtualization/api/hcs/reference/hcsstartcomputesystem
    std::function<decltype(HcsStartComputeSystem)> StartComputeSystem = &HcsStartComputeSystem;
    // @ref https://learn.microsoft.com/en-us/virtualization/api/hcs/reference/hcsshutdowncomputesystem
    std::function<decltype(HcsShutDownComputeSystem)> ShutDownComputeSystem = &HcsShutDownComputeSystem;
    // @ref https://learn.microsoft.com/en-us/virtualization/api/hcs/reference/hcsterminatecomputesystem
    std::function<decltype(HcsTerminateComputeSystem)> TerminateComputeSystem = &HcsTerminateComputeSystem;
    // @ref https://learn.microsoft.com/en-us/virtualization/api/hcs/reference/hcsclosecomputesystem
    std::function<decltype(HcsCloseComputeSystem)> CloseComputeSystem = &HcsCloseComputeSystem;
    // @ref https://learn.microsoft.com/en-us/virtualization/api/hcs/reference/hcspausecomputesystem
    std::function<decltype(HcsPauseComputeSystem)> PauseComputeSystem = &HcsPauseComputeSystem;
    // @ref https://learn.microsoft.com/en-us/virtualization/api/hcs/reference/hcsresumecomputesystem
    std::function<decltype(HcsResumeComputeSystem)> ResumeComputeSystem = &HcsResumeComputeSystem;
    // @ref https://learn.microsoft.com/en-us/virtualization/api/hcs/reference/hcsmodifycomputesystem
    std::function<decltype(HcsModifyComputeSystem)> ModifyComputeSystem = &HcsModifyComputeSystem;
    // @ref https://learn.microsoft.com/en-us/virtualization/api/hcs/reference/hcsgetcomputesystemproperties
    std::function<decltype(HcsGetComputeSystemProperties)> GetComputeSystemProperties = &HcsGetComputeSystemProperties;
    // @ref https://learn.microsoft.com/en-us/virtualization/api/hcs/reference/hcsgrantvmaccess
    std::function<decltype(HcsGrantVmAccess)> GrantVmAccess = &HcsGrantVmAccess;
    // @ref https://learn.microsoft.com/en-us/virtualization/api/hcs/reference/hcsrevokevmaccess
    std::function<decltype(HcsRevokeVmAccess)> RevokeVmAccess = &HcsRevokeVmAccess;
    // @ref https://learn.microsoft.com/en-us/virtualization/api/hcs/reference/hcsenumeratecomputesystems
    std::function<decltype(HcsEnumerateComputeSystems)> EnumerateComputeSystems = &HcsEnumerateComputeSystems;

    /**
     * @brief LocalAlloc/LocalFree is used by the HCS API to manage memory for the status/error
     * messages. It's caller's responsibility to free the messages allocated by the API, that's
     * why the LocalFree is part of the API table.
     *
     * @ref https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-localfree
     */
    std::function<decltype(::LocalFree)> LocalFree = &::LocalFree;
};

} // namespace multipass::hyperv::hcs

/**
 * Formatter type specialization for HCNAPITable
 */
template <typename Char>
struct fmt::formatter<multipass::hyperv::hcs::HCSAPITable, Char>
{
    constexpr auto parse(basic_format_parse_context<Char>& ctx)
    {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const multipass::hyperv::hcs::HCSAPITable& api, FormatContext& ctx) const
    {
        return format_to(ctx.out(),
                         "CreateOperation: ({}) | WaitForOperationResult: ({}) | CreateComputeSystem: ({}) | "
                         "OpenComputeSystem: ({}) | StartComputeSystem: ({}) | ShutDownComputeSystem: ({}) | "
                         "PauseComputeSystem: ({}) | ResumeComputeSystem: ({}) | ModifyComputeSystem: ({}) | "
                         "GetComputeSystemProperties: ({}) | GrantVmAccess: ({}) | RevokeVmAccess: ({}) | "
                         "EnumerateComputeSystems: ({}) | LocalFree: ({})",
                         static_cast<bool>(api.CreateOperation),
                         static_cast<bool>(api.WaitForOperationResult),
                         static_cast<bool>(api.CreateComputeSystem),
                         static_cast<bool>(api.OpenComputeSystem),
                         static_cast<bool>(api.StartComputeSystem),
                         static_cast<bool>(api.ShutDownComputeSystem),
                         static_cast<bool>(api.PauseComputeSystem),
                         static_cast<bool>(api.ResumeComputeSystem),
                         static_cast<bool>(api.ModifyComputeSystem),
                         static_cast<bool>(api.GetComputeSystemProperties),
                         static_cast<bool>(api.GrantVmAccess),
                         static_cast<bool>(api.RevokeVmAccess),
                         static_cast<bool>(api.EnumerateComputeSystems),
                         static_cast<bool>(api.LocalFree));
    }
};

#endif // MULTIPASS_HYPERV_API_HCS_API_TABLE
