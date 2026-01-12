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

#include <hyperv_api/hcs/hyperv_hcs_api.h>

namespace multipass::hyperv::hcs
{

HCSAPI::HCSAPI(const Singleton<HCSAPI>::PrivatePass& pass) noexcept
    : Singleton<HCSAPI>::Singleton{pass}
{
}

HCS_OPERATION HCSAPI::HcsCreateOperation(const void* context,
                                         HCS_OPERATION_COMPLETION callback) const
{
    return ::HcsCreateOperation(context, callback);
}

HRESULT HCSAPI::HcsWaitForOperationResult(HCS_OPERATION operation,
                                          DWORD timeoutMs,
                                          PWSTR* resultDocument) const
{
    return ::HcsWaitForOperationResult(operation, timeoutMs, resultDocument);
}

void HCSAPI::HcsCloseOperation(HCS_OPERATION operation) const
{
    return ::HcsCloseOperation(operation);
}

HRESULT HCSAPI::HcsCreateComputeSystem(PCWSTR id,
                                       PCWSTR configuration,
                                       HCS_OPERATION operation,
                                       const SECURITY_DESCRIPTOR* securityDescriptor,
                                       HCS_SYSTEM* computeSystem) const
{
    return ::HcsCreateComputeSystem(id,
                                    configuration,
                                    operation,
                                    securityDescriptor,
                                    computeSystem);
}

HRESULT HCSAPI::HcsOpenComputeSystem(PCWSTR id,
                                     DWORD requestedAccess,
                                     HCS_SYSTEM* computeSystem) const
{
    return ::HcsOpenComputeSystem(id, requestedAccess, computeSystem);
}

HRESULT HCSAPI::HcsStartComputeSystem(HCS_SYSTEM computeSystem,
                                      HCS_OPERATION operation,
                                      PCWSTR options) const
{
    return ::HcsStartComputeSystem(computeSystem, operation, options);
}

HRESULT HCSAPI::HcsShutDownComputeSystem(HCS_SYSTEM computeSystem,
                                         HCS_OPERATION operation,
                                         PCWSTR options) const
{
    return ::HcsShutDownComputeSystem(computeSystem, operation, options);
}

HRESULT HCSAPI::HcsTerminateComputeSystem(HCS_SYSTEM computeSystem,
                                          HCS_OPERATION operation,
                                          PCWSTR options) const
{
    return ::HcsTerminateComputeSystem(computeSystem, operation, options);
}

void HCSAPI::HcsCloseComputeSystem(HCS_SYSTEM computeSystem) const
{
    return ::HcsCloseComputeSystem(computeSystem);
}

HRESULT HCSAPI::HcsPauseComputeSystem(HCS_SYSTEM computeSystem,
                                      HCS_OPERATION operation,
                                      PCWSTR options) const
{
    return ::HcsPauseComputeSystem(computeSystem, operation, options);
}

HRESULT HCSAPI::HcsResumeComputeSystem(HCS_SYSTEM computeSystem,
                                       HCS_OPERATION operation,
                                       PCWSTR options) const
{
    return ::HcsResumeComputeSystem(computeSystem, operation, options);
}

HRESULT HCSAPI::HcsModifyComputeSystem(HCS_SYSTEM computeSystem,
                                       HCS_OPERATION operation,
                                       PCWSTR configuration,
                                       HANDLE identity) const
{
    return ::HcsModifyComputeSystem(computeSystem, operation, configuration, identity);
}

HRESULT HCSAPI::HcsGetComputeSystemProperties(HCS_SYSTEM computeSystem,
                                              HCS_OPERATION operation,
                                              PCWSTR propertyQuery) const
{
    return ::HcsGetComputeSystemProperties(computeSystem, operation, propertyQuery);
}

HRESULT HCSAPI::HcsGrantVmAccess(PCWSTR vmId, PCWSTR filePath) const
{
    return ::HcsGrantVmAccess(vmId, filePath);
}

HRESULT HCSAPI::HcsRevokeVmAccess(PCWSTR vmId, PCWSTR filePath) const
{
    return ::HcsRevokeVmAccess(vmId, filePath);
}

HRESULT HCSAPI::HcsEnumerateComputeSystems(PCWSTR query, HCS_OPERATION operation) const
{
    return ::HcsEnumerateComputeSystems(query, operation);
}

HRESULT HCSAPI::HcsSetComputeSystemCallback(HCS_SYSTEM computeSystem,
                                            HCS_EVENT_OPTIONS callbackOptions,
                                            const void* context,
                                            HCS_EVENT_CALLBACK callback) const
{
    return ::HcsSetComputeSystemCallback(computeSystem, callbackOptions, context, callback);
}

HRESULT HCSAPI::HcsSaveComputeSystem(HCS_SYSTEM computeSystem,
                                     HCS_OPERATION operation,
                                     PCWSTR options) const
{
    return ::HcsSaveComputeSystem(computeSystem, operation, options);
}

HRESULT HCSAPI::HcsCreateEmptyGuestStateFile(PCWSTR guestStateFilePath) const
{
    return ::HcsCreateEmptyGuestStateFile(guestStateFilePath);
}

HRESULT HCSAPI::HcsCreateEmptyRuntimeStateFile(PCWSTR runtimeStateFilePath) const
{
    return ::HcsCreateEmptyRuntimeStateFile(runtimeStateFilePath);
}

HLOCAL HCSAPI::LocalFree(HLOCAL hMem) const
{
    return ::LocalFree(hMem);
}

} // namespace multipass::hyperv::hcs
