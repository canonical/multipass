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

#include "net_io_api.h"

namespace multipass::hyperv
{
namespace
{
void free_mib_table(MIB_IPNET_TABLE2* table)
{
    ::FreeMibTable(table);
}
} // namespace

NetIOAPI::NetIOAPI(const Singleton<NetIOAPI>::PrivatePass& pass) noexcept
    : Singleton<NetIOAPI>::Singleton{pass}
{
}

void NetIOAPI::InitializeIpInterfaceEntry(PMIB_IPINTERFACE_ROW Row) const
{
    ::InitializeIpInterfaceEntry(Row);
}

DWORD NetIOAPI::SetIpInterfaceEntry(PMIB_IPINTERFACE_ROW Row) const
{
    return ::SetIpInterfaceEntry(Row);
}

DWORD NetIOAPI::ConvertInterfaceAliasToLuid(const WCHAR* InterfaceName,
                                            NET_LUID* InterfaceLuid) const
{
    return ::ConvertInterfaceAliasToLuid(InterfaceName, InterfaceLuid);
}

DWORD NetIOAPI::ConvertInterfaceGuidToLuid(const GUID* InterfaceGuid,
                                           NET_LUID* InterfaceLuid) const
{
    return ::ConvertInterfaceGuidToLuid(InterfaceGuid, InterfaceLuid);
}

IpNetTableResult NetIOAPI::GetIpNetTable2(ADDRESS_FAMILY Family) const
{
    PMIB_IPNET_TABLE2 table{};
    const auto error = ::GetIpNetTable2(Family, &table);
    return {error, IpNetTable{table, &free_mib_table}};
}

DWORD NetIOAPI::DeleteIpNetEntry2(const MIB_IPNET_ROW2* Row) const
{
    return ::DeleteIpNetEntry2(Row);
}
} // namespace multipass::hyperv
