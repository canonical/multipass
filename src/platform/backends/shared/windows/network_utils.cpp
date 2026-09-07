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

#include "net_io_api.h"

#include <multipass/logging/log.h>
#include <multipass/utils.h>

#include <fmt/format.h>

#include <windows.h>
#include <WS2tcpip.h>
#include <WinSock2.h>
#include <iphlpapi.h>
#include <netioapi.h>
#include <winternl.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <cstring>

namespace multipass
{
namespace
{
constexpr auto log_category = "windows-network";
constexpr std::size_t ethernet_address_length = 6;

std::optional<std::array<unsigned char, ethernet_address_length>> physical_address(
    std::string mac_address)
{
    std::ranges::replace(mac_address, '-', ':');
    if (!utils::valid_mac_address(mac_address))
        return std::nullopt;

    std::array<unsigned char, ethernet_address_length> address;
    for (std::size_t index = 0; index < address.size(); ++index)
    {
        unsigned int octet;
        const auto* begin = mac_address.data() + index * 3;
        const auto [end, error] = std::from_chars(begin, begin + 2, octet, 16);
        if (error != std::errc{} || end != begin + 2)
            return std::nullopt;

        address[index] = static_cast<unsigned char>(octet);
    }

    return address;
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

WindowsNetworkUtils::WindowsNetworkUtils(
    const Singleton<WindowsNetworkUtils>::PrivatePass& pass) noexcept
    : Singleton<WindowsNetworkUtils>{pass}
{
}

std::optional<std::string> WindowsNetworkUtils::permanent_ipv4_neighbor(
    const std::string& mac_address) const
{
    const auto mac = physical_address(mac_address);
    if (!mac)
    {
        logging::error(log_category, "Invalid MAC address `{}`", mac_address);
        return std::nullopt;
    }

    auto result = MP_NETIOAPI.GetIpNetTable2(AF_INET);
    if (result.error != NO_ERROR)
    {
        logging::error(log_category, "GetIpNetTable2 failed with error code {}", result.error);
        return std::nullopt;
    }

    const auto matches = [&mac](const MIB_IPNET_ROW2& row) {
        return row.Address.si_family == AF_INET && row.State == NlnsPermanent &&
               row.PhysicalAddressLength == ethernet_address_length &&
               std::memcmp(row.PhysicalAddress, mac->data(), mac->size()) == 0;
    };

    const auto* begin = result.table->Table;
    const auto* end = begin + result.table->NumEntries;
    if (const auto row = std::find_if(begin, end, matches); row != end)
        return ipv4_to_string(row->Address.Ipv4.sin_addr);

    return std::nullopt;
}

} // namespace multipass
