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

#include "dnsmasq_server.h"
#include "firewall_config.h"

#include <qemu_platform.h>

#include <multipass/path.h>

#include <unordered_map>
#include <utility>

namespace multipass
{
// This class is the platform detail for QEMU on Linux
class QemuPlatformDetail : public QemuPlatform
{
public:
    explicit QemuPlatformDetail(const Path& data_dir);
    virtual ~QemuPlatformDetail();

    std::optional<IPAddress> get_ip_for(const std::string& hw_addr) override;
    void remove_resources_for(const std::string& name) override;
    void platform_health_check() override;
    QStringList vm_platform_args(const VirtualMachineDescription& vm_desc) override;
    bool is_network_supported(const std::string& network_type) const override;
    bool needs_network_prep() const override;
    std::string create_bridge_with(const NetworkInterfaceInfo& interface) const override;
    void set_authorization(std::vector<NetworkInterfaceInfo>& networks) override;

private:
    // explicitly naming DisabledCopyMove since the private one derived from QemuPlatform takes
    // precedence in lookup
    struct Subnet : private multipass::DisabledCopyMove
    {
        const QString bridge_name;
        const std::string subnet;
        FirewallConfig::UPtr firewall_config;

        Subnet(const Path& network_dir, const std::string& name);
        ~Subnet();
    };
    using Subnets = std::unordered_map<std::string, Subnet>;

    [[nodiscard]] static Subnets get_subnets(const Path& network_dir);

    [[nodiscard]] static SubnetList get_subnets_list(const Subnets&);

    const Path network_dir;
    const Subnets subnets;
    DNSMasqServer::UPtr dnsmasq_server;
    std::unordered_map<std::string, std::tuple<QString, std::string, QString>>
        name_to_net_device_map;
};
} // namespace multipass
