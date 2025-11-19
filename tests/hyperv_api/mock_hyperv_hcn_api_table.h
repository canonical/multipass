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

#include <hyperv_api/hcn/hyperv_hcn_api_table.h>

#include "../mock_singleton_helpers.h"

namespace multipass::test
{

class MockHCNAPI : public hyperv::hcn::HCNAPI
{
public:
    using HCNAPI::HCNAPI;

    MOCK_METHOD(HRESULT,
                HcnCreateNetwork,
                (REFGUID Id, PCWSTR Settings, PHCN_NETWORK Network, PWSTR* ErrorRecord),
                (const override));
    MOCK_METHOD(HRESULT,
                HcnOpenNetwork,
                (REFGUID Id, PHCN_NETWORK Network, PWSTR* ErrorRecord),
                (const override));
    MOCK_METHOD(HRESULT, HcnDeleteNetwork, (REFGUID Id, PWSTR* ErrorRecord), (const override));
    MOCK_METHOD(HRESULT, HcnCloseNetwork, (HCN_NETWORK Network), (const override));
    MOCK_METHOD(HRESULT,
                HcnCreateEndpoint,
                (HCN_NETWORK Network,
                 REFGUID Id,
                 PCWSTR Settings,
                 PHCN_ENDPOINT Endpoint,
                 PWSTR* ErrorRecord),
                (const override));
    MOCK_METHOD(HRESULT,
                HcnOpenEndpoint,
                (REFGUID Id, PHCN_ENDPOINT Endpoint, PWSTR* ErrorRecord),
                (const override));
    MOCK_METHOD(HRESULT, HcnDeleteEndpoint, (REFGUID Id, PWSTR* ErrorRecord), (const override));
    MOCK_METHOD(HRESULT, HcnCloseEndpoint, (HCN_ENDPOINT Endpoint), (const override));
    MOCK_METHOD(void, CoTaskMemFree, (LPVOID pv), (const override));

    MP_MOCK_SINGLETON_BOILERPLATE(MockHCNAPI, HCNAPI);
};
} // namespace multipass::test
