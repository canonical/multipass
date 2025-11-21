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

#include <hyperv_api/hcs/hyperv_hcs_api_table.h>

#include "tests/mock_singleton_helpers.h"

namespace multipass::test
{

class MockHCSAPI : public hyperv::hcs::HCSAPI
{
public:
    using HCSAPI::HCSAPI;

    MOCK_METHOD(HCS_OPERATION,
                HcsCreateOperation,
                (const void* context, HCS_OPERATION_COMPLETION callback),
                (const, override));

    MOCK_METHOD(HRESULT,
                HcsWaitForOperationResult,
                (HCS_OPERATION operation, DWORD timeoutMs, PWSTR* resultDocument),
                (const, override));

    MOCK_METHOD(void, HcsCloseOperation, (HCS_OPERATION operation), (const, override));

    MOCK_METHOD(HRESULT,
                HcsCreateComputeSystem,
                (PCWSTR id,
                 PCWSTR configuration,
                 HCS_OPERATION operation,
                 const SECURITY_DESCRIPTOR* securityDescriptor,
                 HCS_SYSTEM* computeSystem),
                (const, override));

    MOCK_METHOD(HRESULT,
                HcsOpenComputeSystem,
                (PCWSTR id, DWORD requestedAccess, HCS_SYSTEM* computeSystem),
                (const, override));

    MOCK_METHOD(HRESULT,
                HcsStartComputeSystem,
                (HCS_SYSTEM computeSystem, HCS_OPERATION operation, PCWSTR options),
                (const, override));

    MOCK_METHOD(HRESULT,
                HcsShutDownComputeSystem,
                (HCS_SYSTEM computeSystem, HCS_OPERATION operation, PCWSTR options),
                (const, override));

    MOCK_METHOD(HRESULT,
                HcsTerminateComputeSystem,
                (HCS_SYSTEM computeSystem, HCS_OPERATION operation, PCWSTR options),
                (const, override));

    MOCK_METHOD(void, HcsCloseComputeSystem, (HCS_SYSTEM computeSystem), (const));

    MOCK_METHOD(HRESULT,
                HcsPauseComputeSystem,
                (HCS_SYSTEM computeSystem, HCS_OPERATION operation, PCWSTR options),
                (const, override));

    MOCK_METHOD(HRESULT,
                HcsResumeComputeSystem,
                (HCS_SYSTEM computeSystem, HCS_OPERATION operation, PCWSTR options),
                (const, override));

    MOCK_METHOD(
        HRESULT,
        HcsModifyComputeSystem,
        (HCS_SYSTEM computeSystem, HCS_OPERATION operation, PCWSTR configuration, HANDLE identity),
        (const, override));

    MOCK_METHOD(HRESULT,
                HcsGetComputeSystemProperties,
                (HCS_SYSTEM computeSystem, HCS_OPERATION operation, PCWSTR propertyQuery),
                (const, override));

    MOCK_METHOD(HRESULT, HcsGrantVmAccess, (PCWSTR vmId, PCWSTR filePath), (const, override));

    MOCK_METHOD(HRESULT, HcsRevokeVmAccess, (PCWSTR vmId, PCWSTR filePath), (const, override));

    MOCK_METHOD(HRESULT,
                HcsEnumerateComputeSystems,
                (PCWSTR query, HCS_OPERATION operation),
                (const, override));

    MOCK_METHOD(HRESULT,
                HcsSetComputeSystemCallback,
                (HCS_SYSTEM computeSystem,
                 HCS_EVENT_OPTIONS callbackOptions,
                 const void* context,
                 HCS_EVENT_CALLBACK callback),
                (const, override));

    MOCK_METHOD(HLOCAL, LocalFree, (HLOCAL hMem), (const, override));

    MP_MOCK_SINGLETON_BOILERPLATE(MockHCSAPI, HCSAPI);
};
} // namespace multipass::test
