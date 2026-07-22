/*
 * Copyright (C) Canonical, Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
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

#include "network_utils.h"

#include <multipass/logging/log.h>
#include <multipass/utils.h>

#include <fmt/format.h>

#include <WinSock2.h>
#include <WS2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>
#include <netioapi.h>
#include <winternl.h>

#include <algorithm>
#include <cctype>
#include <memory>

namespace multipass
{
namespace
{
constexpr auto log_category = "windows-network";
constexpr std::size_t ethernet_address_length = 6;

std::string canonical_mac_address(const unsigned char* address)
{
    return fmt::format("{:02x}:{:02x}:{:02x}:{:02x}:{:02x}:{:02x}",
                       address[0],
                       address[1],
                       address[2],
                       address[3],
                       address[4],
                       address[5]);
}

std::string ipv4_to_string(const IN_ADDR& address)
{
    return fmt::format("{}.{}.{}.{}",
                       address.S_un.S_un_b.s_b1,
                       address.S_un.S_un_b.s_b2,
                       address.S_un.S_un_b.s_b3,
                       address.S_un.S_un_b.s_b4);
}
} // namespace

std::optional<std::string> permanent_ipv4_neighbor(const std::string& mac_address)
{
    if (!utils::valid_mac_address(mac_address))
    {
        logging::error(log_category, "Invalid MAC address `{}`", mac_address);
        return std::nullopt;
    }

    PMIB_IPNET_TABLE2 raw_table{};
    if (const auto result = GetIpNetTable2(AF_INET, &raw_table); result != NO_ERROR)
    {
        logging::error(log_category, "GetIpNetTable2 failed with error code {}", result);
        return std::nullopt;
    }

    const std::unique_ptr<MIB_IPNET_TABLE2, decltype(&FreeMibTable)> table{raw_table,
                                                                           &FreeMibTable};
    if (!table)
    {
        logging::error(log_category, "GetIpNetTable2 returned no neighbor table");
        return std::nullopt;
    }

    const auto matches = [&mac_address](const MIB_IPNET_ROW2& row) {
        return row.Address.si_family == AF_INET && row.State == NlnsPermanent &&
               row.PhysicalAddressLength == ethernet_address_length &&
               std::ranges::equal(canonical_mac_address(row.PhysicalAddress),
                                  mac_address,
                                  [](unsigned char lhs, unsigned char rhs) {
                                      return std::tolower(lhs) == std::tolower(rhs);
                                  });
    };

    const auto* begin = table->Table;
    const auto* end = begin + table->NumEntries;
    if (const auto row = std::find_if(begin, end, matches); row != end)
        return ipv4_to_string(row->Address.Ipv4.sin_addr);

    return std::nullopt;
}

} // namespace multipass
