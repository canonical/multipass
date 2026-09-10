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

#include <memory>

#define MP_NETIOAPI multipass::hyperv::NetIOAPI::instance()

namespace multipass::hyperv
{
using IpNetTable = std::unique_ptr<MIB_IPNET_TABLE2, void (*)(MIB_IPNET_TABLE2*)>;

struct IpNetTableResult
{
    DWORD error;
    IpNetTable table;
};

struct NetIOAPI : public Singleton<NetIOAPI>
{
    NetIOAPI(const Singleton<NetIOAPI>::PrivatePass&) noexcept;

    virtual void InitializeIpInterfaceEntry(PMIB_IPINTERFACE_ROW Row) const;
    [[nodiscard]] virtual DWORD SetIpInterfaceEntry(PMIB_IPINTERFACE_ROW Row) const;
    [[nodiscard]] virtual DWORD ConvertInterfaceAliasToLuid(const WCHAR* InterfaceName,
                                                            NET_LUID* InterfaceLuid) const;
    [[nodiscard]] virtual IpNetTableResult GetIpNetTable2(ADDRESS_FAMILY Family) const;
};
} // namespace multipass::hyperv
