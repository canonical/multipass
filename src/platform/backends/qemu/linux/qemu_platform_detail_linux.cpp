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

#include "multipass/constants.h"
#include "qemu_platform_detail.h"

#include <multipass/file_ops.h>
#include <multipass/format.h>
#include <multipass/logging/log.h>
#include <multipass/platform.h>
#include <multipass/utils.h>

#include <shared/linux/backend_utils.h>

#include <QCoreApplication>
#include <QFile>

namespace mp = multipass;
namespace mpl = multipass::logging;
namespace mpu = multipass::utils;

namespace
{
constexpr auto category = "qemu platform";
const QString multipass_bridge_name{"mpqemubr%1"};

// An interface name can only be 15 characters, so this generates a hash of the
// VM instance name with a "tap-" prefix and then truncates it.
auto generate_tap_device_name(const std::string& vm_name)
{
    const auto name_hash = std::hash<std::string>{}(vm_name);
    auto tap_name = fmt::format("tap-{:x}", name_hash);
    tap_name.resize(15);
    return QString::fromStdString(tap_name);
}

void create_tap_device(const QString& tap_name, const QString& bridge_name)
{
    if (!MP_UTILS.run_cmd_for_status("ip", {"addr", "show", tap_name}))
    {
        MP_UTILS.run_cmd_for_status("ip", {"tuntap", "add", tap_name, "mode", "tap"});
        MP_UTILS.run_cmd_for_status("ip", {"link", "set", tap_name, "master", bridge_name});
        MP_UTILS.run_cmd_for_status("ip", {"link", "set", tap_name, "up"});
    }
}

void remove_tap_device(const QString& tap_device_name)
{
    if (MP_UTILS.run_cmd_for_status("ip", {"addr", "show", tap_device_name}))
    {
        MP_UTILS.run_cmd_for_status("ip", {"link", "delete", tap_device_name});
    }
}

void create_virtual_switch(const mp::Subnet& subnet, const QString& bridge_name)
{
    if (!MP_UTILS.run_cmd_for_status("ip", {"addr", "show", bridge_name}))
    {
        const auto mac_address = mp::utils::generate_mac_address();
        const auto cidr = subnet.as_string();
        const auto broadcast = (subnet.max_address() + 1).as_string();

        MP_UTILS.run_cmd_for_status(
            "ip",
            {"link", "add", bridge_name, "address", mac_address.c_str(), "type", "bridge"});
        MP_UTILS.run_cmd_for_status(
            "ip",
            {"address", "add", cidr.c_str(), "dev", bridge_name, "broadcast", broadcast.c_str()});
        MP_UTILS.run_cmd_for_status("ip", {"link", "set", bridge_name, "up"});
    }
}

void set_ip_forward()
{
    // Command line equivalent: "sysctl -w net.ipv4.ip_forward=1"
    QFile ip_forward("/proc/sys/net/ipv4/ip_forward");
    if (!MP_FILEOPS.open(ip_forward, QFile::ReadWrite))
    {
        mpl::warn(category, "Unable to open {}", qUtf8Printable(ip_forward.fileName()));
        return;
    }

    if (MP_FILEOPS.write(ip_forward, "1") < 0)
    {
        mpl::warn(category, "Failed to write to {}", qUtf8Printable(ip_forward.fileName()));
    }
}

mp::DNSMasqServer::UPtr init_nat_network(const mp::Path& network_dir,
                                         const mp::BridgeSubnetList& subnets)
{
    set_ip_forward();
    return MP_DNSMASQ_SERVER_FACTORY.make_dnsmasq_server(network_dir, subnets);
}

void delete_virtual_switch(const QString& bridge_name)
{
    if (MP_UTILS.run_cmd_for_status("ip", {"addr", "show", bridge_name}))
    {
        MP_UTILS.run_cmd_for_status("ip", {"link", "delete", bridge_name});
    }
}
} // namespace

mp::QemuPlatformDetail::Bridge::Bridge(const Subnet& subnet, const std::string& name)
    : bridge_name{multipass_bridge_name.arg(name.c_str())},
      subnet{subnet},
      firewall_config{MP_FIREWALL_CONFIG_FACTORY.make_firewall_config(bridge_name, subnet)}
{
    create_virtual_switch(subnet, bridge_name);
}

mp::QemuPlatformDetail::Bridge::~Bridge()
{
    delete_virtual_switch(bridge_name);
}

[[nodiscard]] mp::QemuPlatformDetail::Bridges mp::QemuPlatformDetail::get_bridges(
    const AvailabilityZoneManager::Zones& zones)
{
    Bridges bridges{};
    bridges.reserve(zones.size());

    for (const auto& zone_ref : zones)
    {
        const auto& zone = zone_ref.get();
        const auto& name = zone.get_name();
        bridges.emplace(std::piecewise_construct,
                        std::forward_as_tuple(name),
                        std::forward_as_tuple(zone.get_subnet(), name));
    }

    return bridges;
}

[[nodiscard]] mp::BridgeSubnetList mp::QemuPlatformDetail::get_bridge_list(const Bridges& subnets)
{
    BridgeSubnetList out{};
    out.reserve(subnets.size());

    for (const auto& [_, subnet] : subnets)
    {
        out.emplace_back(subnet.bridge_name, subnet.subnet);
    }

    return out;
}

mp::QemuPlatformDetail::QemuPlatformDetail(const mp::Path& data_dir,
                                           const AvailabilityZoneManager::Zones& zones)
    : network_dir{MP_UTILS.make_dir(QDir(data_dir), "network")},
      bridges{get_bridges(zones)},
      dnsmasq_server{init_nat_network(network_dir, get_bridge_list(bridges))}
{
}

mp::QemuPlatformDetail::~QemuPlatformDetail()
{
    for (const auto& it : name_to_net_device_map)
    {
        const auto& [tap_device_name, hw_addr, _] = it.second;
        remove_tap_device(tap_device_name);
    }
}

std::optional<mp::IPAddress> mp::QemuPlatformDetail::get_ip_for(const std::string& hw_addr)
{
    return dnsmasq_server->get_ip_for(hw_addr);
}

void mp::QemuPlatformDetail::remove_resources_for(const std::string& name)
{
    auto it = name_to_net_device_map.find(name);
    if (it != name_to_net_device_map.end())
    {
        const auto& [tap_device_name, hw_addr, bridge_name] = it->second;
        dnsmasq_server->release_mac(hw_addr, bridge_name);
        remove_tap_device(tap_device_name);

        name_to_net_device_map.erase(name);
    }
}

void mp::QemuPlatformDetail::platform_health_check()
{
    MP_BACKEND.check_for_kvm_support();
    MP_BACKEND.check_if_kvm_is_in_use();

    dnsmasq_server->check_dnsmasq_running();
    for (const auto& [_, bridge] : bridges)
    {
        bridge.firewall_config->verify_firewall_rules();
    }
}

QStringList mp::QemuPlatformDetail::vm_platform_args(const VirtualMachineDescription& vm_desc)
{
    // Configure and generate the args for the default network interface
    auto tap_device_name = generate_tap_device_name(vm_desc.vm_name);
    const QString& bridge_name = bridges.at(vm_desc.zone).bridge_name;
    create_tap_device(tap_device_name, bridge_name);

    name_to_net_device_map.emplace(
        vm_desc.vm_name,
        std::make_tuple(tap_device_name, vm_desc.default_mac_address, bridge_name));

    QStringList opts;

    // Work around for Xenial where UEFI images are not one and the same
    if (!(vm_desc.image.original_release == "16.04 LTS" &&
          vm_desc.image.image_path.contains("disk1.img")))
    {
#if defined Q_PROCESSOR_X86
        opts << "-bios"
             << "OVMF.fd";
#elif defined Q_PROCESSOR_ARM
        opts << "-bios"
             << "QEMU_EFI.fd"
             << "-machine"
             << "virt";
#endif
    }

    opts << "--enable-kvm"
         // Pass host CPU flags to VM
         << "-cpu"
         << "host"
         // Set up the network related args
         << "-nic"
         << QString::fromStdString(
                fmt::format("tap,ifname={},script=no,downscript=no,model=virtio-net-pci,mac={}",
                            tap_device_name,
                            vm_desc.default_mac_address));

    const auto bridge_helper_exec_path =
        QDir(QCoreApplication::applicationDirPath()).filePath(BRIDGE_HELPER_EXEC_NAME_CPP);

    for (const auto& extra_interface : vm_desc.extra_interfaces)
    {
        opts << "-nic"
             << QString::fromStdString(
                    fmt::format("bridge,br={},model=virtio-net-pci,mac={},helper={}",
                                extra_interface.id,
                                extra_interface.mac_address,
                                bridge_helper_exec_path));
    }

    return opts;
}

mp::QemuPlatform::UPtr mp::QemuPlatformFactory::make_qemu_platform(
    const Path& data_dir,
    const mp::AvailabilityZoneManager::Zones& zones) const
{
    return std::make_unique<mp::QemuPlatformDetail>(data_dir, zones);
}

bool mp::QemuPlatformDetail::is_network_supported(const std::string& network_type) const
{
    return network_type == "bridge" || network_type == "ethernet";
}

bool mp::QemuPlatformDetail::needs_network_prep() const
{
    return true;
}

void mp::QemuPlatformDetail::set_authorization(std::vector<NetworkInterfaceInfo>& networks)
{
    const auto& br_nomenclature = MP_PLATFORM.bridge_nomenclature();

    for (auto& net : networks)
        if (net.type == "ethernet" && !mpu::find_bridge_with(networks, net.id, br_nomenclature))
            net.needs_authorization = true;
}

std::string mp::QemuPlatformDetail::create_bridge_with(const NetworkInterfaceInfo& interface) const
{
    assert(interface.type == "ethernet");
    return MP_BACKEND.create_bridge_with(interface.id);
}
