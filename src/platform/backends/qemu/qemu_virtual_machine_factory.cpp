/*
 * Copyright (C) 2017-2018 Canonical, Ltd.
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
 * Authored by: Alberto Aguirre <alberto.aguirre@canonical.com>
 *
 */

#include "qemu_virtual_machine_factory.h"
#include "qemu_virtual_machine.h"

#include <multipass/backend_utils.h>
#include <multipass/optional.h>
#include <multipass/utils.h>
#include <multipass/virtual_machine_description.h>

#include <fmt/format.h>
#include <yaml-cpp/yaml.h>

#include <QTcpSocket>

namespace mp = multipass;

namespace
{
// An interface name can only be 15 characters, so this generates a hash of the
// VM instance name with a "tap-" prefix and then truncates it.
auto generate_tap_device_name(const std::string& vm_name)
{
    const auto name_hash = std::hash<std::string>{}(vm_name);
    auto tap_name = fmt::format("tap-{:x}", name_hash);
    tap_name.resize(15);
    return tap_name;
}

auto virtual_switch_subnet()
{
    return mp::utils::run_cmd_for_output("bash",
                                         {"-c", "ip route show | grep mpbr0 | cut -d ' ' -f1 | cut -d '.' -f1-3"});
}

void create_virtual_switch(const std::string& subnet)
{
    if (!mp::utils::run_cmd_for_status("ip", {"addr", "show", "mpbr0"}))
    {
        const auto mac_address = mp::backend::generate_mac_address();
        const auto cidr = fmt::format("{}.1/24", subnet);
        const auto broadcast = fmt::format("{}.255", subnet);

        mp::utils::run_cmd_for_status("ip",
                                      {"link", "add", "mpbr0-dummy", "address", mac_address.c_str(), "type", "dummy"});
        mp::utils::run_cmd_for_status("ip", {"link", "add", "mpbr0", "type", "bridge"});
        mp::utils::run_cmd_for_status("ip", {"link", "set", "mpbr0-dummy", "master", "mpbr0"});
        mp::utils::run_cmd_for_status("ip",
                                      {"address", "add", cidr.c_str(), "dev", "mpbr0", "broadcast", broadcast.c_str()});
        mp::utils::run_cmd_for_status("ip", {"link", "set", "mpbr0", "up"});
    }
}

void delete_virtual_switch()
{
    if (mp::utils::run_cmd_for_status("ip", {"addr", "show", "mpbr0"}))
    {
        mp::utils::run_cmd_for_status("ip", {"link", "delete", "mpbr0"});
        mp::utils::run_cmd_for_status("ip", {"link", "delete", "mpbr0-dummy"});
    }
}

void create_tap_device(const QString& tap_name)
{
    if (!mp::utils::run_cmd_for_status("ip", {"addr", "show", tap_name}))
    {
        mp::utils::run_cmd_for_status("ip", {"tuntap", "add", tap_name, "mode", "tap"});
        mp::utils::run_cmd_for_status("ip", {"link", "set", tap_name, "master", "mpbr0"});
        mp::utils::run_cmd_for_status("ip", {"link", "set", tap_name, "up"});
    }
}

void set_ip_forward()
{
    mp::utils::run_cmd_for_status("sysctl", {"-w", "net.ipv4.ip_forward=1"});
}

void set_nat_iptables(const std::string& subnet)
{
    const auto cidr = QString::fromStdString(fmt::format("{}.0/24", subnet));

    // Setup basic iptables overrides for DHCP/DNS
    if (!mp::utils::run_cmd_for_status("iptables",
                                       {"-C", "INPUT", "-i", "mpbr0", "-p", "udp", "--dport", "67", "-j", "ACCEPT"}))
        mp::utils::run_cmd_for_status("iptables",
                                      {"-I", "INPUT", "-i", "mpbr0", "-p", "udp", "--dport", "67", "-j", "ACCEPT"});

    if (!mp::utils::run_cmd_for_status("iptables",
                                       {"-C", "INPUT", "-i", "mpbr0", "-p", "udp", "--dport", "53", "-j", "ACCEPT"}))
        mp::utils::run_cmd_for_status("iptables",
                                      {"-I", "INPUT", "-i", "mpbr0", "-p", "udp", "--dport", "53", "-j", "ACCEPT"});

    if (!mp::utils::run_cmd_for_status("iptables",
                                       {"-C", "INPUT", "-i", "mpbr0", "-p", "tcp", "--dport", "53", "-j", "ACCEPT"}))
        mp::utils::run_cmd_for_status("iptables",
                                      {"-I", "INPUT", "-i", "mpbr0", "-p", "tcp", "--dport", "53", "-j", "ACCEPT"});

    if (!mp::utils::run_cmd_for_status("iptables",
                                       {"-C", "OUTPUT", "-o", "mpbr0", "-p", "udp", "--sport", "67", "-j", "ACCEPT"}))
        mp::utils::run_cmd_for_status("iptables",
                                      {"-I", "OUTPUT", "-o", "mpbr0", "-p", "udp", "--sport", "67", "-j", "ACCEPT"});

    if (!mp::utils::run_cmd_for_status("iptables",
                                       {"-C", "OUTPUT", "-o", "mpbr0", "-p", "udp", "--sport", "53", "-j", "ACCEPT"}))
        mp::utils::run_cmd_for_status("iptables",
                                      {"-I", "OUTPUT", "-o", "mpbr0", "-p", "udp", "--sport", "53", "-j", "ACCEPT"});

    if (!mp::utils::run_cmd_for_status("iptables",
                                       {"-C", "OUTPUT", "-o", "mpbr0", "-p", "tcp", "--sport", "53", "-j", "ACCEPT"}))
        mp::utils::run_cmd_for_status("iptables",
                                      {"-I", "OUTPUT", "-o", "mpbr0", "-p", "tcp", "--sport", "53", "-j", "ACCEPT"});

    if (!mp::utils::run_cmd_for_status("iptables",
                                       {"-t", "mangle", "-C", "POSTROUTING", "-o", "mpbr0", "-p", "udp", "--dport",
                                        "68", "-j", "CHECKSUM", "--checksum-fill"}))
        mp::utils::run_cmd_for_status("iptables",
                                      {"-t", "mangle", "-I", "POSTROUTING", "-o", "mpbr0", "-p", "udp", "--dport", "68",
                                       "-j", "CHECKSUM", "--checksum-fill"});

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
    if (!mp::utils::run_cmd_for_status("iptables",
                                       {"-t", "nat", "-C", "POSTROUTING", "-s", cidr, "!", "-d", cidr, "-p", "tcp",
                                        "-j", "MASQUERADE", "--to-ports", "1024-65535"}))
        mp::utils::run_cmd_for_status("iptables",
                                      {"-t", "nat", "-I", "POSTROUTING", "-s", cidr, "!", "-d", cidr, "-p", "tcp", "-j",
                                       "MASQUERADE", "--to-ports", "1024-65535"});

    if (!mp::utils::run_cmd_for_status("iptables",
                                       {"-t", "nat", "-C", "POSTROUTING", "-s", cidr, "!", "-d", cidr, "-p", "udp",
                                        "-j", "MASQUERADE", "--to-ports", "1024-65535"}))
        mp::utils::run_cmd_for_status("iptables",
                                      {"-t", "nat", "-I", "POSTROUTING", "-s", cidr, "!", "-d", cidr, "-p", "udp", "-j",
                                       "MASQUERADE", "--to-ports", "1024-65535"});

    if (!mp::utils::run_cmd_for_status(
            "iptables", {"-t", "nat", "-C", "POSTROUTING", "-s", cidr, "!", "-d", cidr, "-j", "MASQUERADE"}))
        mp::utils::run_cmd_for_status(
            "iptables", {"-t", "nat", "-I", "POSTROUTING", "-s", cidr, "!", "-d", cidr, "-j", "MASQUERADE"});

    // Allow established traffic to the private subnet
    if (!mp::utils::run_cmd_for_status("iptables",
                                       {"-C", "FORWARD", "-d", cidr, "-o", "mpbr0", "-m", "conntrack", "--ctstate",
                                        "RELATED,ESTABLISHED", "-j", "ACCEPT"}))
        mp::utils::run_cmd_for_status("iptables",
                                      {"-I", "FORWARD", "-d", cidr, "-o", "mpbr0", "-m", "conntrack", "--ctstate",
                                       "RELATED,ESTABLISHED", "-j", "ACCEPT"});

    // Allow outbound traffic from the private subnet
    if (!mp::utils::run_cmd_for_status("iptables", {"-C", "FORWARD", "-s", cidr, "-i", "mpbr0", "-j", "ACCEPT"}))
        mp::utils::run_cmd_for_status("iptables", {"-I", "FORWARD", "-s", cidr, "-i", "mpbr0", "-j", "ACCEPT"});

    // Allow traffic between virtual machines
    if (!mp::utils::run_cmd_for_status("iptables", {"-C", "FORWARD", "-i", "mpbr0", "-o", "mpbr0", "-j", "ACCEPT"}))
        mp::utils::run_cmd_for_status("iptables", {"-I", "FORWARD", "-i", "mpbr0", "-o", "mpbr0", "-j", "ACCEPT"});

    // Reject everything else
    if (!mp::utils::run_cmd_for_status(
            "iptables", {"-C", "FORWARD", "-i", "mpbr0", "-j", "REJECT", "--reject-with icmp-port-unreachable"}))
        mp::utils::run_cmd_for_status(
            "iptables", {"-I", "FORWARD", "-i", "mpbr0", "-j", "REJECT", "--reject-with icmp-port-unreachable"});

    if (!mp::utils::run_cmd_for_status(
            "iptables", {"-C", "FORWARD", "-o", "mpbr0", "-j", "REJECT", "--reject-with icmp-port-unreachable"}))
        mp::utils::run_cmd_for_status(
            "iptables", {"-I", "FORWARD", "-o", "mpbr0", "-j", "REJECT", "--reject-with icmp-port-unreachable"});
}

std::string make_subnet(bool use_legacy_subnet)
{
    if (use_legacy_subnet)
        return "10.122.122";

    return mp::backend::generate_random_subnet();
}

std::string get_subnet(const mp::Path& data_dir, bool use_legacy_subnet)
{
    auto subnet = virtual_switch_subnet();
    if (!subnet.empty())
        return subnet;

    QFile subnet_file{data_dir + "/vm-ips/multipass_subnet"};
    subnet_file.open(QIODevice::ReadWrite | QIODevice::Text);
    if (subnet_file.size() > 0)
        return subnet_file.readAll().trimmed().toStdString();

    auto new_subnet = make_subnet(use_legacy_subnet);
    subnet_file.write(new_subnet.data(), new_subnet.length());
    return new_subnet;
}

mp::DNSMasqServer create_dnsmasq_server(const mp::Path& data_dir, mp::optional<mp::IPAddress> first_ip)
{
    const bool use_legacy_subnet = first_ip ? true : false;
    const auto subnet = get_subnet(data_dir, use_legacy_subnet);

    create_virtual_switch(subnet);
    set_ip_forward();
    set_nat_iptables(subnet);

    const auto bridge_addr = mp::IPAddress{fmt::format("{}.1", subnet)};
    const auto start_addr = mp::IPAddress{fmt::format("{}.2", subnet)};
    const auto end_addr = mp::IPAddress{fmt::format("{}.254", subnet)};
    return {data_dir, bridge_addr, first_ip.value_or(start_addr), end_addr};
}
}

mp::QemuVirtualMachineFactory::QemuVirtualMachineFactory(const mp::Path& data_dir)
    : legacy_ip_pool{data_dir, mp::IPAddress{"10.122.122.2"}, mp::IPAddress{"10.122.122.254"}},
      dnsmasq_server{create_dnsmasq_server(data_dir, legacy_ip_pool.first_free_ip())}
{
}

mp::QemuVirtualMachineFactory::~QemuVirtualMachineFactory()
{
    delete_virtual_switch();
}

mp::VirtualMachine::UPtr mp::QemuVirtualMachineFactory::create_virtual_machine(const VirtualMachineDescription& desc,
                                                                               VMStatusMonitor& monitor)
{
    auto tap_device_name = generate_tap_device_name(desc.vm_name);
    create_tap_device(QString::fromStdString(tap_device_name));

    return std::make_unique<mp::QemuVirtualMachine>(desc, legacy_ip_pool.check_ip_for(desc.vm_name), tap_device_name,
                                                    mp::backend::generate_mac_address(), dnsmasq_server, monitor);
}

void mp::QemuVirtualMachineFactory::remove_resources_for(const std::string& name)
{
    legacy_ip_pool.remove_ip_for(name);
}

mp::FetchType mp::QemuVirtualMachineFactory::fetch_type()
{
    return mp::FetchType::ImageOnly;
}

mp::VMImage mp::QemuVirtualMachineFactory::prepare_source_image(const mp::VMImage& source_image)
{
    return source_image;
}

void mp::QemuVirtualMachineFactory::prepare_instance_image(const mp::VMImage& instance_image,
                                                           const VirtualMachineDescription& desc)
{
    mp::backend::resize_instance_image(desc.disk_space, instance_image.image_path);
}

void mp::QemuVirtualMachineFactory::configure(const std::string& name, YAML::Node& meta_config, YAML::Node& user_config)
{
}

void mp::QemuVirtualMachineFactory::check_hypervisor_support()
{
    mp::backend::check_hypervisor_support();
}
