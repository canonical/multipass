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
        mpl::log(mpl::Level::warning, "daemon",
                 fmt::format("Unable to open {}", qUtf8Printable(ip_forward.fileName())));
        return;
    }

    if (ip_forward.write("1") < 0)
    {
        mpl::log(mpl::Level::warning, "daemon",
                 fmt::format("Failed to write to {}", qUtf8Printable(ip_forward.fileName())));
    }
}

mp::DNSMasqServer create_dnsmasq_server(const mp::Path& network_dir, const QString& bridge_name,
                                        const std::string& subnet)
{
    create_virtual_switch(subnet, bridge_name);
    set_ip_forward();

    return {network_dir, bridge_name, subnet};
}
} // namespace

mp::QemuVirtualMachineFactory::QemuVirtualMachineFactory(const mp::Path& data_dir)
    : bridge_name{QString::fromStdString(multipass_bridge_name)},
      network_dir{mp::utils::make_dir(QDir(data_dir), "network")},
      subnet{mp::backend::get_subnet(network_dir, bridge_name)},
      dnsmasq_server{create_dnsmasq_server(network_dir, bridge_name, subnet)},
      iptables_config{bridge_name, subnet}
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

    dnsmasq_server.check_dnsmasq_running();
    iptables_config.verify_iptables_rules();
}

QString mp::QemuVirtualMachineFactory::get_backend_version_string()
{
    auto process =
        mp::ProcessFactory::instance().create_process("qemu-system-" + mp::backend::cpu_arch(), {"--version"});

    auto version_re = QRegularExpression("^QEMU emulator version ([\\d\\.]+)");
    auto exit_state = process->execute();

    if (exit_state.completed_successfully())
    {
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
