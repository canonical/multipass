/*
 * Copyright (C) 2017-2019 Canonical, Ltd.
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

#include "qemu_virtual_machine_factory.h"
#include "qemu_virtual_machine.h"

#include <multipass/logging/log.h>
#include <multipass/optional.h>
#include <multipass/utils.h>
#include <multipass/virtual_machine_description.h>

#include <shared/linux/backend_utils.h>
#include <shared/linux/process_factory.h>

#include <multipass/format.h>
#include <yaml-cpp/yaml.h>

#include <QRegularExpression>
#include <QTcpSocket>

namespace mp = multipass;
namespace mpl = multipass::logging;

namespace
{
constexpr auto multipass_bridge_name = "mpqemubr0";

// An interface name can only be 15 characters, so this generates a hash of the
// VM instance name with a "tap-" prefix and then truncates it.
auto generate_tap_device_name(const std::string& vm_name)
{
    const auto name_hash = std::hash<std::string>{}(vm_name);
    auto tap_name = fmt::format("tap-{:x}", name_hash);
    tap_name.resize(15);
    return tap_name;
}

void create_virtual_switch(const std::string& subnet, const QString& bridge_name)
{
    const QString dummy_name{bridge_name + "-dummy"};

    if (!mp::utils::run_cmd_for_status("ip", {"addr", "show", bridge_name}))
    {
        const auto mac_address = mp::utils::generate_mac_address();
        const auto cidr = fmt::format("{}.1/24", subnet);
        const auto broadcast = fmt::format("{}.255", subnet);

        mp::utils::run_cmd_for_status("ip",
                                      {"link", "add", dummy_name, "address", mac_address.c_str(), "type", "dummy"});
        mp::utils::run_cmd_for_status("ip", {"link", "add", bridge_name, "type", "bridge"});
        mp::utils::run_cmd_for_status("ip", {"link", "set", dummy_name, "master", bridge_name});
        mp::utils::run_cmd_for_status(
            "ip", {"address", "add", cidr.c_str(), "dev", bridge_name, "broadcast", broadcast.c_str()});
        mp::utils::run_cmd_for_status("ip", {"link", "set", bridge_name, "up"});
    }
}

void delete_virtual_switch(const QString& bridge_name)
{
    const QString dummy_name{bridge_name + "-dummy"};

    if (mp::utils::run_cmd_for_status("ip", {"addr", "show", bridge_name}))
    {
        mp::utils::run_cmd_for_status("ip", {"link", "delete", bridge_name});
        mp::utils::run_cmd_for_status("ip", {"link", "delete", dummy_name});
    }
}

void create_tap_device(const QString& tap_name, const QString& bridge_name)
{
    if (!mp::utils::run_cmd_for_status("ip", {"addr", "show", tap_name}))
    {
        mp::utils::run_cmd_for_status("ip", {"tuntap", "add", tap_name, "mode", "tap"});
        mp::utils::run_cmd_for_status("ip", {"link", "set", tap_name, "master", bridge_name});
        mp::utils::run_cmd_for_status("ip", {"link", "set", tap_name, "up"});
    }
}

void set_ip_forward()
{
    // Command line equivalent: "sysctl -w net.ipv4.ip_forward=1"
    QFile ip_forward("/proc/sys/net/ipv4/ip_forward");
    if (!ip_forward.open(QFile::ReadWrite))
    {
        mpl::log(mpl::Level::warning, "daemon", fmt::format("Unable to open {}", qPrintable(ip_forward.fileName())));
        return;
    }

    if (ip_forward.write("1") < 0)
    {
        mpl::log(mpl::Level::warning, "daemon",
                 fmt::format("Failed to write to {}", qPrintable(ip_forward.fileName())));
    }
}

void set_nat_iptables(const std::string& subnet, const QString& bridge_name)
{
    const auto cidr = QString::fromStdString(fmt::format("{}.0/24", subnet));

    // Setup basic iptables overrides for DHCP/DNS
    if (!mp::utils::run_cmd_for_status(
            "iptables", {"-C", "INPUT", "-i", bridge_name, "-p", "udp", "--dport", "67", "-j", "ACCEPT"}))
        mp::utils::run_cmd_for_status("iptables",
                                      {"-I", "INPUT", "-i", bridge_name, "-p", "udp", "--dport", "67", "-j", "ACCEPT"});

    if (!mp::utils::run_cmd_for_status(
            "iptables", {"-C", "INPUT", "-i", bridge_name, "-p", "udp", "--dport", "53", "-j", "ACCEPT"}))
        mp::utils::run_cmd_for_status("iptables",
                                      {"-I", "INPUT", "-i", bridge_name, "-p", "udp", "--dport", "53", "-j", "ACCEPT"});

    if (!mp::utils::run_cmd_for_status(
            "iptables", {"-C", "INPUT", "-i", bridge_name, "-p", "tcp", "--dport", "53", "-j", "ACCEPT"}))
        mp::utils::run_cmd_for_status("iptables",
                                      {"-I", "INPUT", "-i", bridge_name, "-p", "tcp", "--dport", "53", "-j", "ACCEPT"});

    if (!mp::utils::run_cmd_for_status(
            "iptables", {"-C", "OUTPUT", "-o", bridge_name, "-p", "udp", "--sport", "67", "-j", "ACCEPT"}))
        mp::utils::run_cmd_for_status(
            "iptables", {"-I", "OUTPUT", "-o", bridge_name, "-p", "udp", "--sport", "67", "-j", "ACCEPT"});

    if (!mp::utils::run_cmd_for_status(
            "iptables", {"-C", "OUTPUT", "-o", bridge_name, "-p", "udp", "--sport", "53", "-j", "ACCEPT"}))
        mp::utils::run_cmd_for_status(
            "iptables", {"-I", "OUTPUT", "-o", bridge_name, "-p", "udp", "--sport", "53", "-j", "ACCEPT"});

    if (!mp::utils::run_cmd_for_status(
            "iptables", {"-C", "OUTPUT", "-o", bridge_name, "-p", "tcp", "--sport", "53", "-j", "ACCEPT"}))
        mp::utils::run_cmd_for_status(
            "iptables", {"-I", "OUTPUT", "-o", bridge_name, "-p", "tcp", "--sport", "53", "-j", "ACCEPT"});

    if (!mp::utils::run_cmd_for_status("iptables", {"-t", "mangle", "-C", "POSTROUTING", "-o", bridge_name, "-p", "udp",
                                                    "--dport", "68", "-j", "CHECKSUM", "--checksum-fill"}))
        mp::utils::run_cmd_for_status("iptables", {"-t", "mangle", "-I", "POSTROUTING", "-o", bridge_name, "-p", "udp",
                                                   "--dport", "68", "-j", "CHECKSUM", "--checksum-fill"});

    // Do not masquerade to these reserved address blocks.
    if (!mp::utils::run_cmd_for_status(
            "iptables", {"-t", "nat", "-C", "POSTROUTING", "-s", cidr, "-d", "224.0.0.0/24", "-j", "RETURN"}))
        mp::utils::run_cmd_for_status(
            "iptables", {"-t", "nat", "-I", "POSTROUTING", "-s", cidr, "-d", "224.0.0.0/24", "-j", "RETURN"});

    if (!mp::utils::run_cmd_for_status(
            "iptables", {"-t", "nat", "-C", "POSTROUTING", "-s", cidr, "-d", "255.255.255.255/32", "-j", "RETURN"}))
        mp::utils::run_cmd_for_status(
            "iptables", {"-t", "nat", "-I", "POSTROUTING", "-s", cidr, "-d", "255.255.255.255/32", "-j", "RETURN"});

    // Masquerade all packets going from VMs to the LAN/Internet
    if (!mp::utils::run_cmd_for_status("iptables", {"-t", "nat", "-C", "POSTROUTING", "-s", cidr, "!", "-d", cidr, "-p",
                                                    "tcp", "-j", "MASQUERADE", "--to-ports", "1024-65535"}))
        mp::utils::run_cmd_for_status("iptables", {"-t", "nat", "-I", "POSTROUTING", "-s", cidr, "!", "-d", cidr, "-p",
                                                   "tcp", "-j", "MASQUERADE", "--to-ports", "1024-65535"});

    if (!mp::utils::run_cmd_for_status("iptables", {"-t", "nat", "-C", "POSTROUTING", "-s", cidr, "!", "-d", cidr, "-p",
                                                    "udp", "-j", "MASQUERADE", "--to-ports", "1024-65535"}))
        mp::utils::run_cmd_for_status("iptables", {"-t", "nat", "-I", "POSTROUTING", "-s", cidr, "!", "-d", cidr, "-p",
                                                   "udp", "-j", "MASQUERADE", "--to-ports", "1024-65535"});

    if (!mp::utils::run_cmd_for_status(
            "iptables", {"-t", "nat", "-C", "POSTROUTING", "-s", cidr, "!", "-d", cidr, "-j", "MASQUERADE"}))
        mp::utils::run_cmd_for_status(
            "iptables", {"-t", "nat", "-I", "POSTROUTING", "-s", cidr, "!", "-d", cidr, "-j", "MASQUERADE"});

    // Allow established traffic to the private subnet
    if (!mp::utils::run_cmd_for_status("iptables", {"-C", "FORWARD", "-d", cidr, "-o", bridge_name, "-m", "conntrack",
                                                    "--ctstate", "RELATED,ESTABLISHED", "-j", "ACCEPT"}))
        mp::utils::run_cmd_for_status("iptables", {"-I", "FORWARD", "-d", cidr, "-o", bridge_name, "-m", "conntrack",
                                                   "--ctstate", "RELATED,ESTABLISHED", "-j", "ACCEPT"});

    // Allow outbound traffic from the private subnet
    if (!mp::utils::run_cmd_for_status("iptables", {"-C", "FORWARD", "-s", cidr, "-i", bridge_name, "-j", "ACCEPT"}))
        mp::utils::run_cmd_for_status("iptables", {"-I", "FORWARD", "-s", cidr, "-i", bridge_name, "-j", "ACCEPT"});

    // Allow traffic between virtual machines
    if (!mp::utils::run_cmd_for_status("iptables",
                                       {"-C", "FORWARD", "-i", bridge_name, "-o", bridge_name, "-j", "ACCEPT"}))
        mp::utils::run_cmd_for_status("iptables",
                                      {"-I", "FORWARD", "-i", bridge_name, "-o", bridge_name, "-j", "ACCEPT"});

    // Reject everything else
    if (!mp::utils::run_cmd_for_status(
            "iptables", {"-C", "FORWARD", "-i", bridge_name, "-j", "REJECT", "--reject-with icmp-port-unreachable"}))
        mp::utils::run_cmd_for_status(
            "iptables", {"-I", "FORWARD", "-i", bridge_name, "-j", "REJECT", "--reject-with icmp-port-unreachable"});

    if (!mp::utils::run_cmd_for_status(
            "iptables", {"-C", "FORWARD", "-o", bridge_name, "-j", "REJECT", "--reject-with icmp-port-unreachable"}))
        mp::utils::run_cmd_for_status(
            "iptables", {"-I", "FORWARD", "-o", bridge_name, "-j", "REJECT", "--reject-with icmp-port-unreachable"});
}

mp::DNSMasqServer create_dnsmasq_server(const mp::Path& data_dir, const QString& bridge_name)
{
    auto network_dir = mp::utils::make_dir(QDir(data_dir), "network");
    auto subnet = mp::backend::get_subnet(network_dir, bridge_name);

    create_virtual_switch(subnet, bridge_name);
    if (subnet.empty()) // if bridge didn't exist, need to get subnet again
    {
        subnet = mp::backend::get_subnet(network_dir, bridge_name);
    }
    set_ip_forward();
    set_nat_iptables(subnet, bridge_name);

    const auto bridge_addr = mp::IPAddress{fmt::format("{}.1", subnet)};
    const auto start_addr = mp::IPAddress{fmt::format("{}.2", subnet)};
    const auto end_addr = mp::IPAddress{fmt::format("{}.254", subnet)};
    return {network_dir, bridge_name, bridge_addr, start_addr, end_addr};
}
} // namespace

mp::QemuVirtualMachineFactory::QemuVirtualMachineFactory(const mp::Path& data_dir)
    : bridge_name{QString::fromStdString(multipass_bridge_name)},
      dnsmasq_server{create_dnsmasq_server(data_dir, bridge_name)}
{
}

mp::QemuVirtualMachineFactory::~QemuVirtualMachineFactory()
{
    delete_virtual_switch(bridge_name);
}

mp::VirtualMachine::UPtr mp::QemuVirtualMachineFactory::create_virtual_machine(const VirtualMachineDescription& desc,
                                                                               VMStatusMonitor& monitor)
{
    auto tap_device_name = generate_tap_device_name(desc.vm_name);
    create_tap_device(QString::fromStdString(tap_device_name), bridge_name);

    auto vm = std::make_unique<mp::QemuVirtualMachine>(desc, tap_device_name, dnsmasq_server, monitor);

    name_to_mac_map.emplace(desc.vm_name, desc.mac_addr);
    return vm;
}

void mp::QemuVirtualMachineFactory::remove_resources_for(const std::string& name)
{
    auto it = name_to_mac_map.find(name);
    if (it != name_to_mac_map.end())
    {
        dnsmasq_server.release_mac(it->second);
    }
}

mp::FetchType mp::QemuVirtualMachineFactory::fetch_type()
{
    return mp::FetchType::ImageOnly;
}

mp::VMImage mp::QemuVirtualMachineFactory::prepare_source_image(const mp::VMImage& source_image)
{
    VMImage image{source_image};
    image.image_path = mp::backend::convert_to_qcow_if_necessary(source_image.image_path);
    return image;
}

void mp::QemuVirtualMachineFactory::prepare_instance_image(const mp::VMImage& instance_image,
                                                           const VirtualMachineDescription& desc)
{
    mp::backend::resize_instance_image(desc.disk_space, instance_image.image_path);
}

void mp::QemuVirtualMachineFactory::configure(const std::string& /*name*/, YAML::Node& /*meta_config*/,
                                              YAML::Node& /*user_config*/)
{
}

void mp::QemuVirtualMachineFactory::hypervisor_health_check()
{
    mp::backend::check_for_kvm_support();
    mp::backend::check_if_kvm_is_in_use();
}

QString mp::QemuVirtualMachineFactory::get_backend_version_string()
{
    auto process =
        mp::ProcessFactory::instance().create_process("qemu-system-" + mp::backend::cpu_arch(), {"--version"});

    auto exit_state = process->execute();

    if (exit_state.completed_successfully())
    {
        auto version_re = QRegularExpression("^QEMU emulator version ([\\d\\.]+)");
        auto match = version_re.match(process->read_all_standard_output());

        if (match.hasMatch())
            return QString("qemu-%1").arg(match.captured(1));
        else
        {
            mpl::log(mpl::Level::error, "daemon",
                     fmt::format("Failed to parse QEMU version out: '{}'", process->read_all_standard_output()));
            return QString("qemu-unknown");
        }
    }
    else
    {
        if (exit_state.error)
        {
            mpl::log(mpl::Level::error, "daemon",
                     fmt::format("Qemu failed to start: {}", exit_state.failure_message()));
        }
        else if (exit_state.exit_code)
        {
            mpl::log(mpl::Level::error, "daemon",
                     fmt::format("Qemu fail: '{}' with outputs:\n{}\n{}", exit_state.failure_message(),
                                 process->read_all_standard_output(), process->read_all_standard_error()));
        }
    }

    return QString("qemu-unknown");
}
