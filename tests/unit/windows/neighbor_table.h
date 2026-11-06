/*
 * Copyright (C) Canonical, Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "shared/windows/net_io_api.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <initializer_list>

namespace multipass::test
{
struct NeighborRow
{
    std::array<unsigned char, 4> address;
    ULONG64 interface_luid;
    std::array<unsigned char, 6> mac{0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};
};

// A successful GetIpNetTable2 result with a permanent IPv4 entry per row.
inline hyperv::IpNetTableResult make_neighbor_table(std::initializer_list<NeighborRow> rows)
{
    const auto row_count = std::max<std::size_t>(rows.size(), 1);
    const auto size = sizeof(MIB_IPNET_TABLE2) + (row_count - 1) * sizeof(MIB_IPNET_ROW2);
    auto* table = reinterpret_cast<MIB_IPNET_TABLE2*>(new std::byte[size]{});
    table->NumEntries = static_cast<ULONG>(rows.size());

    std::size_t index = 0;
    for (const auto& [address, interface_luid, mac] : rows)
    {
        auto& row = table->Table[index++];
        row.InterfaceLuid.Value = interface_luid;
        row.Address.Ipv4.sin_family = AF_INET;
        row.Address.Ipv4.sin_addr.S_un.S_un_b = {address[0], address[1], address[2], address[3]};
        row.State = NlnsPermanent;
        row.PhysicalAddressLength = static_cast<ULONG>(mac.size());
        std::ranges::copy(mac, row.PhysicalAddress);
    }

    return {NO_ERROR, hyperv::IpNetTable{table, [](MIB_IPNET_TABLE2* table) {
                                             delete[] reinterpret_cast<std::byte*>(table);
                                         }}};
}
} // namespace multipass::test
