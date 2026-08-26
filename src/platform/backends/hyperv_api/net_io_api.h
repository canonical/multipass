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

#include <ws2tcpip.h>

#include <iphlpapi.h>

#define MP_NETIOAPI multipass::hyperv::NetIOAPI::instance()

namespace multipass::hyperv
{

struct NetIOAPI : public Singleton<NetIOAPI>
{
    NetIOAPI(const Singleton<NetIOAPI>::PrivatePass&) noexcept;

    virtual void InitializeIpInterfaceEntry(PMIB_IPINTERFACE_ROW Row) const;
    [[nodiscard]] virtual DWORD SetIpInterfaceEntry(PMIB_IPINTERFACE_ROW Row) const;
    [[nodiscard]] virtual DWORD ConvertInterfaceAliasToLuid(const WCHAR* InterfaceName,
                                                            NET_LUID* InterfaceLuid) const;
};

} // namespace multipass::hyperv
