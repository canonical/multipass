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

#include <multipass/virtual_machine_description.h>

#include <yaml-cpp/yaml.h>

#include <chrono>
#include <functional>
#include <iomanip>
#include <ios>
#include <random>
#include <sstream>

#include <QProcess>
#include <QTcpSocket>

namespace mp = multipass;

namespace
{
auto generate_mac_address()
{
    std::stringstream mac_addr("52:54:00", std::ios_base::app | std::ios_base::out);

    unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
    std::default_random_engine generator(seed);
    std::uniform_int_distribution<int> distribution(0, 255);

    mac_addr << std::setfill('0');

    for (auto i = 0; i < 3; ++i)
    {
        mac_addr << ":" << std::setw(2) << std::hex << distribution(generator);
    }

    return mac_addr.str();
}

// An interface name can only be 15 characters, so this generates a hash of the
// VM instance name with a "tap-" prefix and then truncates it.
auto generate_tap_device_name(const std::string& vm_name)
{
    auto name_hash = std::hash<std::string>{}(vm_name);
    std::stringstream hash_stream("tap-", std::ios_base::app | std::ios_base::out);

    hash_stream << std::hex << name_hash;
    auto hash_str = hash_stream.str();
    hash_str.resize(15);

    return hash_str;
}

bool run_network_cmd(QString cmd, QStringList args)
{
    QProcess network_cmd;
    network_cmd.setProgram(cmd);
    network_cmd.setArguments(args);

    network_cmd.start();
    network_cmd.waitForFinished();

    return network_cmd.exitStatus() == QProcess::NormalExit && network_cmd.exitCode() == 0;
}

void create_virtual_switch()
{
    if (!run_network_cmd("ip", {"addr", "show", "mpbr0"}))
    {
        run_network_cmd(
            "ip",
            {"link", "add", "mpbr0-dummy", "address", QString::fromStdString(generate_mac_address()), "type", "dummy"});
        run_network_cmd("ip", {"link", "add", "mpbr0", "type", "bridge"});
        run_network_cmd("ip", {"link", "set", "mpbr0-dummy", "master", "mpbr0"});
        run_network_cmd("ip", {"address", "add", "10.122.122.1/24", "dev", "mpbr0", "broadcast", "10.122.122.255"});
        run_network_cmd("ip", {"link", "set", "mpbr0", "up"});
    }
}

void create_tap_device(const QString& tap_name)
{
    if (!run_network_cmd("ip", {"addr", "show", tap_name}))
    {
        run_network_cmd("ip", {"tuntap", "add", tap_name, "mode", "tap"});
        run_network_cmd("ip", {"link", "set", tap_name, "master", "mpbr0"});
        run_network_cmd("ip", {"link", "set", tap_name, "up"});
    }
}

void remove_tap_device(const std::string& name)
{
    auto tap_device_name = QString::fromStdString(generate_tap_device_name(name));

    if (run_network_cmd("ip", {"addr", "show", tap_device_name}))
    {
        run_network_cmd("ip", {"link", "delete", tap_device_name});
    }
}

void set_ip_forward()
{
    run_network_cmd("sysctl", {"-w", "net.ipv4.ip_forward=1"});
}

void set_nat_iptables()
{
    // Do not masquerade to these reserved address blocks.
    if (!run_network_cmd(
            "iptables",
            {"-t", "nat", "-C", "POSTROUTING", "-s", "10.122.122.0/24", "-d", "224.0.0.0/24", "-j", "RETURN"}))
        run_network_cmd(
            "iptables",
            {"-t", "nat", "-I", "POSTROUTING", "-s", "10.122.122.0/24", "-d", "224.0.0.0/24", "-j", "RETURN"});

    if (!run_network_cmd(
            "iptables",
            {"-t", "nat", "-C", "POSTROUTING", "-s", "10.122.122.0/24", "-d", "255.255.255.255/32", "-j", "RETURN"}))
        run_network_cmd(
            "iptables",
            {"-t", "nat", "-I", "POSTROUTING", "-s", "10.122.122.0/24", "-d", "255.255.255.255/32", "-j", "RETURN"});

    // Masquerade all packets going from VMs to the LAN/Internet
    if (!run_network_cmd("iptables",
                         {"-t", "nat", "-C", "POSTROUTING", "-s", "10.122.122.0/24", "!", "-d", "10.122.122.0/24", "-p",
                          "tcp", "-j", "MASQUERADE", "--to-ports", "1024-65535"}))
        run_network_cmd("iptables",
                        {"-t", "nat", "-I", "POSTROUTING", "-s", "10.122.122.0/24", "!", "-d", "10.122.122.0/24", "-p",
                         "tcp", "-j", "MASQUERADE", "--to-ports", "1024-65535"});

    if (!run_network_cmd("iptables",
                         {"-t", "nat", "-C", "POSTROUTING", "-s", "10.122.122.0/24", "!", "-d", "10.122.122.0/24", "-p",
                          "udp", "-j", "MASQUERADE", "--to-ports", "1024-65535"}))
        run_network_cmd("iptables",
                        {"-t", "nat", "-I", "POSTROUTING", "-s", "10.122.122.0/24", "!", "-d", "10.122.122.0/24", "-p",
                         "udp", "-j", "MASQUERADE", "--to-ports", "1024-65535"});

    if (!run_network_cmd("iptables",
                         {"-t", "nat", "-C", "POSTROUTING", "-s", "10.122.122.0/24", "!", "-d", "10.122.122.0/24", "-j",
                          "MASQUERADE"}))
        run_network_cmd("iptables",
                        {"-t", "nat", "-I", "POSTROUTING", "-s", "10.122.122.0/24", "!", "-d", "10.122.122.0/24", "-j",
                         "MASQUERADE"});

    // Allow established traffic to the private subnet
    if (!run_network_cmd("iptables",
                         {"-C", "FORWARD", "-d", "10.122.122.0/24", "-o", "mpbr0", "-m", "conntrack", "--ctstate",
                          "RELATED,ESTABLISHED", "-j", "ACCEPT"}))
        run_network_cmd("iptables",
                        {"-I", "FORWARD", "-d", "10.122.122.0/24", "-o", "mpbr0", "-m", "conntrack", "--ctstate",
                         "RELATED,ESTABLISHED", "-j", "ACCEPT"});

    // Allow outbound traffic from the private subnet
    if (!run_network_cmd("iptables", {"-C", "FORWARD", "-s", "10.122.122.0/24", "-i", "mpbr0", "-j", "ACCEPT"}))
        run_network_cmd("iptables", {"-I", "FORWARD", "-s", "10.122.122.0/24", "-i", "mpbr0", "-j", "ACCEPT"});

    // Allow traffic between virtual machines
    if (!run_network_cmd("iptables", {"-C", "FORWARD", "-i", "mpbr0", "-o", "mpbr0", "-j", "ACCEPT"}))
        run_network_cmd("iptables", {"-I", "FORWARD", "-i", "mpbr0", "-o", "mpbr0", "-j", "ACCEPT"});

    // Reject everything else
    if (!run_network_cmd("iptables",
                         {"-C", "FORWARD", "-i", "mpbr0", "-j", "REJECT", "--reject-with icmp-port-unreachable"}))
        run_network_cmd("iptables",
                        {"-I", "FORWARD", "-i", "mpbr0", "-j", "REJECT", "--reject-with icmp-port-unreachable"});

    if (!run_network_cmd("iptables",
                         {"-C", "FORWARD", "-o", "mpbr0", "-j", "REJECT", "--reject-with icmp-port-unreachable"}))
        run_network_cmd("iptables",
                        {"-I", "FORWARD", "-o", "mpbr0", "-j", "REJECT", "--reject-with icmp-port-unreachable"});
}
}

mp::QemuVirtualMachineFactory::QemuVirtualMachineFactory(const mp::Path& data_dir)
    : ip_pool{data_dir, mp::IPAddress{"10.122.122.2"}, mp::IPAddress{"10.122.122.254"}},
      dnsmasq_server{data_dir, ip_pool.first_free_ip(), mp::IPAddress{"10.122.122.254"}}
{
}

mp::VirtualMachine::UPtr mp::QemuVirtualMachineFactory::create_virtual_machine(const VirtualMachineDescription& desc,
                                                                               VMStatusMonitor& monitor)
{
    create_virtual_switch();

    auto tap_device_name = generate_tap_device_name(desc.vm_name);
    create_tap_device(QString::fromStdString(tap_device_name));

    set_ip_forward();
    set_nat_iptables();

    return std::make_unique<mp::QemuVirtualMachine>(desc, ip_pool.check_ip_for(desc.vm_name), tap_device_name,
                                                    generate_mac_address(), dnsmasq_server, monitor);
}

void mp::QemuVirtualMachineFactory::remove_resources_for(const std::string& name)
{
    ip_pool.remove_ip_for(name);

    remove_tap_device(name);
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
    if (!desc.disk_space.empty())
    {
        QProcess resize_image;

        QStringList resize_image_args(
            {QStringLiteral("resize"), instance_image.image_path, QString::fromStdString(desc.disk_space)});

        resize_image.start("qemu-img", resize_image_args);
        resize_image.waitForFinished();
    }
}

void mp::QemuVirtualMachineFactory::configure(const std::string& name, YAML::Node& meta_config, YAML::Node& user_config)
{
}

void mp::QemuVirtualMachineFactory::check_hypervisor_support()
{
    QProcess cmd;
    cmd.start("bash",
              QStringList() << "-c"
                            << "egrep -m1 -w '^flags[[:blank:]]*:' /proc/cpuinfo | egrep -wo '(vmx|svm)'");
    cmd.waitForFinished();
    auto virt_type = cmd.readAll().trimmed();

    if (virt_type.isEmpty())
        throw std::runtime_error("CPU does not support KVM extensions");

    if (!QFile::exists("/dev/kvm"))
    {
        std::string kvm_module = (virt_type == "vmx") ? "kvm_intel" : "kvm_amd";
        throw std::runtime_error("/dev/kvm does not exist. Try 'sudo modprobe " + kvm_module + "'.");
    }
}
