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
#include <computenetwork.h>
#include <objbase.h> // for CoTaskMemFree

namespace multipass::hyperv::hcn
{

struct HCNAPI : public Singleton<HCNAPI>
{
    HCNAPI(const Singleton<HCNAPI>::PrivatePass&) noexcept;

    [[nodiscard]] virtual HRESULT HcnCreateNetwork(REFGUID Id,
                                                   PCWSTR Settings,
                                                   PHCN_NETWORK Network,
                                                   PWSTR* ErrorRecord) const;
    [[nodiscard]] virtual HRESULT HcnOpenNetwork(REFGUID Id,
                                                 PHCN_NETWORK Network,
                                                 PWSTR* ErrorRecord) const;
    [[nodiscard]] virtual HRESULT HcnDeleteNetwork(REFGUID Id, PWSTR* ErrorRecord) const;
    [[nodiscard]] virtual HRESULT HcnCloseNetwork(HCN_NETWORK Network) const;
    [[nodiscard]] virtual HRESULT HcnCreateEndpoint(HCN_NETWORK Network,
                                                    REFGUID Id,
                                                    PCWSTR Settings,
                                                    PHCN_ENDPOINT Endpoint,
                                                    PWSTR* ErrorRecord) const;
    [[nodiscard]] virtual HRESULT HcnOpenEndpoint(REFGUID Id,
                                                  PHCN_ENDPOINT Endpoint,
                                                  PWSTR* ErrorRecord) const;
    [[nodiscard]] virtual HRESULT HcnDeleteEndpoint(REFGUID Id, PWSTR* ErrorRecord) const;
    [[nodiscard]] virtual HRESULT HcnCloseEndpoint(HCN_ENDPOINT Endpoint) const;
    [[nodiscard]] virtual HRESULT HcnEnumerateEndpoints(PCWSTR Query, PWSTR* Endpoints, PWSTR* ErrorRecord) const;
    virtual void CoTaskMemFree(LPVOID pv) const;
};

} // namespace multipass::hyperv::hcn
