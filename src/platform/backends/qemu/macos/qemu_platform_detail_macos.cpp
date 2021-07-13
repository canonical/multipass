/*
 * Copyright (C) 2021 Canonical, Ltd.
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

#include "qemu_platform_detail.h"

#include <multipass/format.h>
#include <multipass/logging/log.h>
#include <multipass/utils.h>

#include <QCoreApplication>

#include <fstream>
#include <regex>

namespace mp = multipass;
namespace mpl = multipass::logging;

mp::optional<mp::IPAddress> mp::QemuPlatformDetail::get_ip_for(const std::string& hw_addr)
{
    std::ifstream leases_file{"/var/db/dhcpd_leases"};

    const std::regex hw_addr_re{"\\s*hw_address=\\d+,(.+)"};
    const std::regex ipv4_re{"\\s*ip_address=(.+)"};
    const std::regex known_lines{"^\\s*($|\\}$|name=|hw_address=|identifier=|lease=)"};

    const auto orig_hw_addr_tokens{mp::utils::split(hw_addr, ":")};
    std::string line;
    std::smatch match;
    bool hw_addr_matched = false;
    mp::optional<mp::IPAddress> ip_address;

    while (getline(leases_file, line))
    {
        if (line == "{")
        {
            hw_addr_matched = false;
            ip_address = mp::nullopt;
        }
        else if (regex_match(line, match, hw_addr_re))
        {
            const auto found_hw_addr_tokens{mp::utils::split(match[1], ":")};
            auto found_it = found_hw_addr_tokens.cbegin();

            if (std::all_of(orig_hw_addr_tokens.cbegin(), orig_hw_addr_tokens.cend(),
                            [&found_it](const auto& orig_token) {
                                return stoi(orig_token, 0, 16) == stoi(*found_it++, 0, 16);
                            }))
            {
                hw_addr_matched = true;
            }
        }
        else if (regex_match(line, match, ipv4_re))
        {
            ip_address.emplace(match[1]);
        }
        else if (line == "}" && hw_addr_matched && !ip_address)
            throw std::runtime_error("Failed to parse IP address out of the leases file.");
        else if (!regex_search(line, known_lines))
            mpl::log(mpl::Level::warning, "qemu",
                     fmt::format("Got unexpected line when parsing the leases file: {}", line));

        if (hw_addr_matched && ip_address)
            return ip_address;
    }
    return mp::nullopt;
}

void mp::QemuPlatformDetail::remove_resources_for(const std::string& name)
{
}

void mp::QemuPlatformDetail::platform_health_check()
{
}

QStringList mp::QemuPlatformDetail::full_platform_args(const VirtualMachineDescription& vm_desc)
{
    return QStringList() << "-accel"
                         << "hvf"
                         << "-drive"
                         << QString("file=%1/../Resources/qemu/edk2-x86_64-code.fd,if=pflash,format=raw,readonly=on")
                                .arg(QCoreApplication::applicationDirPath())
                         // Set up the network related args
                         << "-nic"
                         << QString::fromStdString(fmt::format("vmnet-macos,mode=shared,model=virtio-net-pci,mac={}", vm_desc.default_mac_address));
}

QString mp::QemuPlatformDetail::get_directory_name()
{
    return "qemu";
}

mp::QemuPlatform::UPtr mp::QemuPlatformFactory::make_qemu_platform(const Path& data_dir) const
{
    return std::make_unique<mp::QemuPlatformDetail>();
}
