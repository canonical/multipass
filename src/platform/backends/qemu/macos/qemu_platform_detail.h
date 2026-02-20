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

#include <qemu_platform.h>

#include <multipass/availability_zone_manager.h>
#include <multipass/path.h>

#include <unordered_map>

namespace multipass
{
// This class is the platform detail for QEMU on macOS
class QemuPlatformDetail : public QemuPlatform
{
public:
    explicit QemuPlatformDetail(const AvailabilityZoneManager::Zones& zones);

    std::optional<IPAddress> get_ip_for(const std::string& hw_addr) override;
    void remove_resources_for(const std::string& name) override;
    void platform_health_check() override;
    QStringList vmstate_platform_args() override;
    QStringList vm_platform_args(const VirtualMachineDescription& vm_desc) override;
    QString get_directory_name() const override;
    bool is_network_supported(const std::string& network_type) const override;
    bool needs_network_prep() const override;
    std::string create_bridge_with(const NetworkInterfaceInfo& interface) const override;
    void set_authorization(std::vector<NetworkInterfaceInfo>& networks) override;

private:
    using Bridges = std::unordered_map<std::string, Subnet>;

    [[nodiscard]] static Bridges get_bridges(const AvailabilityZoneManager::Zones& zones);

    const QString host_arch{HOST_ARCH};
    const QStringList common_args;
    const Bridges bridges;
};
} // namespace multipass
