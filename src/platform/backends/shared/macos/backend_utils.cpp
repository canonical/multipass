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

#include "backend_utils.h"

#include <multipass/platform.h>
#include <multipass/process/simple_process_spec.h>

namespace mp = multipass;

namespace
{
QString simplify_mac_address(const QString& input_mac_address)
{
    // Trim the (first) leading 0 of each segment of the a MAC address.
    // For example: "04:54:00:b9:69:b5" -> "4:54:0:b9:69:b5"
    QString result_mac_address = input_mac_address;

    // Handle the middle segments: Replace ":0" with ":"
    result_mac_address.replace(":0", ":");

    // Handle the first segment: Remove leading zero if it exists
    if (result_mac_address.startsWith('0'))
    {
        // 0 is the start index and 1 is the lengh of the sub-string to remove
        result_mac_address.remove(0, 1);
    }

    return result_mac_address;
}

QString get_arp_output()
{
    // -a shows all Address Resolution Protocol(ARP) entries, -n shows numeric IP addresses instead
    // of resolving to hostnames
    const auto arp_process = mp::platform::make_process(mp::simple_process_spec("arp", {"-an"}));
    const auto arp_exit_state = arp_process->execute();

    if (!arp_exit_state.completed_successfully())
    {
        throw std::runtime_error(fmt::format("arp failed ({}) with the following output:\n{}",
                                             arp_exit_state.failure_message(),
                                             arp_process->read_all_standard_error()));
    }

    return QString{arp_process->read_all_standard_output()};
}

bool ping_ip(const mp::IPAddress& ip_addr)
{
    constexpr auto timeout = 500;
    const auto arp_process = mp::platform::make_process(
        mp::simple_process_spec("ping", {"-c", "1", QString::fromStdString(ip_addr.as_string())}));
    const auto arp_exit_state = arp_process->execute(timeout);

    return arp_exit_state.completed_successfully();
}

} // namespace

std::optional<mp::IPAddress> mp::backend::get_neighbour_ip(const std::string& mac_address)
{
    // Example output:
    // ? (192.168.1.1) at 3c:37:86:8a:e6:84 on en0 ifscope [ethernet]
    // ? (192.168.1.255) at ff:ff:ff:ff:ff:ff on en0 ifscope [ethernet]
    // ? (192.168.64.2) at 52:54:0:2a:12:b6 on bridge100 ifscope [bridge]
    // ? (192.168.64.3) at 52:54:0:85:72:55 on bridge100 ifscope [bridge]
    // ? (192.168.64.4) at 52:54:0:e1:cd:ab on bridge100 ifscope [bridge]
    // ? (192.168.64.255) at ff:ff:ff:ff:ff:ff on bridge100 ifscope [bridge]
    // ? (224.0.0.251) at 1:0:5e:0:0:fb on en0 ifscope permanent [ethernet]

    const QString arp_output = get_arp_output();
    const QRegularExpression ip_and_mac_address_pair_regex(R"(\(([^)\s]+)\) at ([^\s]+))");
    QRegularExpressionMatchIterator iter = ip_and_mac_address_pair_regex.globalMatch(arp_output);

    const QString arp_format_mac_address =
        simplify_mac_address(QString::fromStdString(mac_address));

    std::optional<mp::IPAddress> best_match;

    while (iter.hasNext())
    {
        QRegularExpressionMatch match = iter.next();
        // group 0 is the full match, 1 and 2 are the inner groups
        if (match.captured(2) == arp_format_mac_address)
        {
            mp::IPAddress current_ip{match.captured(1).toStdString()};

            if ((!best_match.has_value() || current_ip > best_match.value()) && ping_ip(current_ip))
            {
                best_match = current_ip;
            }
        }
    }

    return best_match;
}
