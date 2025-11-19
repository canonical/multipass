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

#include <hyperv_api/hcn/hyperv_hcn_api_table.h>

namespace multipass::hyperv::hcn
{

HCNAPI::HCNAPI(const Singleton<HCNAPI>::PrivatePass& pass) noexcept
    : Singleton<HCNAPI>::Singleton{pass}
{
}

HRESULT HCNAPI::HcnCreateNetwork(REFGUID Id,
                                 PCWSTR Settings,
                                 PHCN_NETWORK Network,
                                 PWSTR* ErrorRecord) const
{
    return ::HcnCreateNetwork(Id, Settings, Network, ErrorRecord);
}
HRESULT HCNAPI::HcnOpenNetwork(REFGUID Id, PHCN_NETWORK Network, PWSTR* ErrorRecord) const
{
    return ::HcnOpenNetwork(Id, Network, ErrorRecord);
}
HRESULT HCNAPI::HcnDeleteNetwork(REFGUID Id, PWSTR* ErrorRecord) const
{
    return ::HcnDeleteNetwork(Id, ErrorRecord);
}
HRESULT HCNAPI::HcnCloseNetwork(HCN_NETWORK Network) const
{
    return ::HcnCloseNetwork(Network);
}
HRESULT HCNAPI::HcnCreateEndpoint(HCN_NETWORK Network,
                                  REFGUID Id,
                                  PCWSTR Settings,
                                  PHCN_ENDPOINT Endpoint,
                                  PWSTR* ErrorRecord) const
{
    return ::HcnCreateEndpoint(Network, Id, Settings, Endpoint, ErrorRecord);
}
HRESULT HCNAPI::HcnOpenEndpoint(REFGUID Id, PHCN_ENDPOINT Endpoint, PWSTR* ErrorRecord) const
{
    return ::HcnOpenEndpoint(Id, Endpoint, ErrorRecord);
}
HRESULT HCNAPI::HcnDeleteEndpoint(REFGUID Id, PWSTR* ErrorRecord) const
{
    return ::HcnDeleteEndpoint(Id, ErrorRecord);
}
HRESULT HCNAPI::HcnCloseEndpoint(HCN_ENDPOINT Endpoint) const
{
    return ::HcnCloseEndpoint(Endpoint);
}
void HCNAPI::CoTaskMemFree(LPVOID pv) const
{
    ::CoTaskMemFree(pv);
}

} // namespace multipass::hyperv::hcn
