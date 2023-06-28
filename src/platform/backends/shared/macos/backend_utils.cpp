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

#include <multipass/exceptions/ip_unavailable_exception.h>
#include <multipass/file_ops.h>
#include <multipass/format.h>
#include <multipass/logging/log.h>
#include <multipass/utils.h>

#include <algorithm>
#include <regex>

namespace mp = multipass;
namespace mpl = multipass::logging;

// identifier can either be the MAC address or the VM name
std::optional<mp::IPAddress> mp::backend::get_vmnet_dhcp_ip_for(const std::string& identifier)
{
    // bootpd leases entries consist of:
    // {
    //        name=<name>
    //        ip_address=<ipv4>
    //        hw_address=1,<mac addr>
    //        identifier=1,<mac addr>
    //        lease=<lease expiration timestamp in hex>
    // }

    QFile leases_file{"/var/db/dhcpd_leases"};
    const std::regex name_re{fmt::format("\\s*name={}", identifier)};
    const std::regex hw_addr_re{"\\s*hw_address=\\d+,(.+)"};
    const std::regex ipv4_re{"\\s*ip_address=(.+)"};
    const std::regex known_lines{"^\\s*($|\\}$|name=|hw_address=|identifier=|lease=)"};

    const auto orig_hw_addr_tokens{mp::utils::split(identifier, ":")};
    const bool is_hw_addr{(orig_hw_addr_tokens.size() > 1) ? true : false};
    std::smatch match;
    bool identifier_matched = false;
    std::optional<mp::IPAddress> ip_address;

    if (!MP_FILEOPS.open(leases_file, QIODevice::ReadOnly | QIODevice::Text))
        throw IPUnavailableException{fmt::format("Cannot open dhcpd_leases file: {}", leases_file.errorString())};

    QTextStream input{&leases_file};
    auto input_line = MP_FILEOPS.read_line(input);
    while (!input_line.isNull())
    {
        auto line{input_line.toStdString()};
        if (line == "{")
        {
            identifier_matched = false;
            ip_address = std::nullopt;
        }
        else if (!is_hw_addr && !identifier_matched && std::regex_match(line, name_re))
        {
            identifier_matched = true;
        }
        else if (is_hw_addr && !identifier_matched && std::regex_match(line, match, hw_addr_re))
        {
            const auto found_hw_addr_tokens{mp::utils::split(match[1], ":")};
            auto found_it = found_hw_addr_tokens.cbegin();

            if (std::all_of(orig_hw_addr_tokens.cbegin(), orig_hw_addr_tokens.cend(),
                            [&found_it](const auto& orig_token) {
                                return stoi(orig_token, 0, 16) == stoi(*found_it++, 0, 16);
                            }))
            {
                identifier_matched = true;
            }
        }
        else if (std::regex_match(line, match, ipv4_re))
        {
            ip_address.emplace(match[1]);
        }
        else if (line == "}" && identifier_matched && !ip_address)
        {
            throw IPUnavailableException{"Failed to parse IP address out of the leases file."};
        }
        else if (!std::regex_search(line, known_lines))
        {
            mpl::log(mpl::Level::warning, "utils",
                     fmt::format("Got unexpected line when parsing the leases file: {}", line));
        }

        if (identifier_matched && ip_address)
            return ip_address;

        input_line = MP_FILEOPS.read_line(input);
    }
    return std::nullopt;
}
