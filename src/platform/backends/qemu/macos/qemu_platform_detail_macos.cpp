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

#include "qemu_platform_detail.h"

#include <multipass/format.h>
#include <multipass/platform.h>

#include <shared/macos/backend_utils.h>

#include <QCoreApplication>

namespace mp = multipass;

namespace
{
auto get_common_args(const QString& host_arch)
{
    QStringList qemu_args;

    if (host_arch == "aarch64")
    {
        qemu_args << "-machine"
                  << "virt,gic-version=3";
    }

    return qemu_args;
}
} // namespace

mp::QemuPlatformDetail::QemuPlatformDetail() : common_args{get_common_args(host_arch)}
{
}

std::optional<mp::IPAddress> mp::QemuPlatformDetail::get_ip_for(const std::string& hw_addr)
{
    return mp::backend::get_vmnet_dhcp_ip_for(hw_addr);
}

void mp::QemuPlatformDetail::remove_resources_for(const std::string& name)
{
}

void mp::QemuPlatformDetail::platform_health_check()
{
    // TODO: Add appropriate health checks to ensure the QEMU backend will work as expected
}

QStringList mp::QemuPlatformDetail::vmstate_platform_args()
{
    return common_args;
}

QStringList mp::QemuPlatformDetail::vm_platform_args(const VirtualMachineDescription& vm_desc)
{
    QStringList qemu_args;

    qemu_args << common_args << "-accel"
              << "hvf"
              << "-drive"
              << QString("file=%1/../Resources/qemu/edk2-%2-code.fd,if=pflash,format=raw,readonly=on")
                     .arg(QCoreApplication::applicationDirPath())
                     .arg(host_arch)
              << "-cpu"
              << "host"
              // Set up the network related args
              << "-nic"
              << QString::fromStdString(
                     fmt::format("vmnet-shared,model=virtio-net-pci,mac={}", vm_desc.default_mac_address));

    for (const auto& extra_interface : vm_desc.extra_interfaces)
    {
        qemu_args << "-nic"
                  << QString::fromStdString(fmt::format("vmnet-bridged,ifname={},model=virtio-net-pci,mac={}",
                                                        extra_interface.id, extra_interface.mac_address));
    }

    return qemu_args;
}

QString mp::QemuPlatformDetail::get_directory_name()
{
    return "qemu";
}

std::vector<mp::NetworkInterfaceInfo> mp::QemuPlatformDetail::networks() const
{
    auto platform_ifs_info = MP_PLATFORM.get_network_interfaces_info();

    std::vector<NetworkInterfaceInfo> networks;

    for (const auto& ifs_info : platform_ifs_info)
    {
        networks.push_back(ifs_info.second);
    }

    return networks;
}

mp::QemuPlatform::UPtr mp::QemuPlatformFactory::make_qemu_platform(const Path& data_dir) const
{
    return std::make_unique<mp::QemuPlatformDetail>();
}
