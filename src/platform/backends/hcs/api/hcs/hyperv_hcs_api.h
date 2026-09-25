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
#include <computecore.h>

#include <fmt/format.h>

namespace multipass::hyperv::hcs
{

struct HCSAPI : public Singleton<HCSAPI>
{
    HCSAPI(const Singleton<HCSAPI>::PrivatePass&) noexcept;

    [[nodiscard]] virtual HCS_OPERATION HcsCreateOperation(const void* context,
                                                           HCS_OPERATION_COMPLETION callback) const;
    [[nodiscard]] virtual HRESULT HcsWaitForOperationResult(HCS_OPERATION operation,
                                                            DWORD timeoutMs,
                                                            PWSTR* resultDocument) const;
    virtual void HcsCloseOperation(HCS_OPERATION operation) const;
    [[nodiscard]] virtual HRESULT HcsCreateComputeSystem(
        PCWSTR id,
        PCWSTR configuration,
        HCS_OPERATION operation,
        const SECURITY_DESCRIPTOR* securityDescriptor,
        HCS_SYSTEM* computeSystem) const;
    [[nodiscard]] virtual HRESULT HcsOpenComputeSystem(PCWSTR id,
                                                       DWORD requestedAccess,
                                                       HCS_SYSTEM* computeSystem) const;
    [[nodiscard]] virtual HRESULT HcsStartComputeSystem(HCS_SYSTEM computeSystem,
                                                        HCS_OPERATION operation,
                                                        PCWSTR options) const;
    [[nodiscard]] virtual HRESULT HcsShutDownComputeSystem(HCS_SYSTEM computeSystem,
                                                           HCS_OPERATION operation,
                                                           PCWSTR options) const;
    [[nodiscard]] virtual HRESULT HcsTerminateComputeSystem(HCS_SYSTEM computeSystem,
                                                            HCS_OPERATION operation,
                                                            PCWSTR options) const;
    virtual void HcsCloseComputeSystem(HCS_SYSTEM computeSystem) const;
    [[nodiscard]] virtual HRESULT HcsPauseComputeSystem(HCS_SYSTEM computeSystem,
                                                        HCS_OPERATION operation,
                                                        PCWSTR options) const;
    [[nodiscard]] virtual HRESULT HcsResumeComputeSystem(HCS_SYSTEM computeSystem,
                                                         HCS_OPERATION operation,
                                                         PCWSTR options) const;
    [[nodiscard]] virtual HRESULT HcsModifyComputeSystem(HCS_SYSTEM computeSystem,
                                                         HCS_OPERATION operation,
                                                         PCWSTR configuration,
                                                         HANDLE identity) const;
    [[nodiscard]] virtual HRESULT HcsGetComputeSystemProperties(HCS_SYSTEM computeSystem,
                                                                HCS_OPERATION operation,
                                                                PCWSTR propertyQuery) const;
    [[nodiscard]] virtual HRESULT HcsGrantVmAccess(PCWSTR vmId, PCWSTR filePath) const;
    [[nodiscard]] virtual HRESULT HcsRevokeVmAccess(PCWSTR vmId, PCWSTR filePath) const;
    [[nodiscard]] virtual HRESULT HcsEnumerateComputeSystems(PCWSTR query,
                                                             HCS_OPERATION operation) const;
    [[nodiscard]] virtual HRESULT HcsSetComputeSystemCallback(HCS_SYSTEM computeSystem,
                                                              HCS_EVENT_OPTIONS callbackOptions,
                                                              const void* context,
                                                              HCS_EVENT_CALLBACK callback) const;
    [[nodiscard]] virtual HRESULT HcsSaveComputeSystem(HCS_SYSTEM computeSystem,
                                                       HCS_OPERATION operation,
                                                       PCWSTR options) const;
    [[nodiscard]] virtual HRESULT HcsCreateEmptyGuestStateFile(PCWSTR guestStateFilePath) const;
    [[nodiscard]] virtual HRESULT HcsCreateEmptyRuntimeStateFile(PCWSTR runtimeStateFilePath) const;

    virtual HLOCAL LocalFree(HLOCAL hMem) const;
};

} // namespace multipass::hyperv::hcs
