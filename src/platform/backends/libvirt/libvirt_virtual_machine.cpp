/*
 * Copyright (C) 2018-2019 Canonical, Ltd.
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

#include "libvirt_virtual_machine.h"

#include <multipass/backend_utils.h>
#include <multipass/logging/log.h>
#include <multipass/optional.h>
#include <multipass/ssh/ssh_session.h>
#include <multipass/utils.h>
#include <multipass/virtual_machine_description.h>
#include <multipass/vm_status_monitor.h>

#include <QDir>
#include <QXmlStreamReader>

#include <fmt/format.h>

#include <libvirt/virterror.h>

namespace mp = multipass;
namespace mpl = multipass::logging;

namespace
{
virErrorPtr libvirt_error;

void libvirt_error_handler(void* opaque, virErrorPtr error)
{
    virFreeError(libvirt_error);
    libvirt_error = virSaveLastError();
}

auto instance_mac_addr_for(virDomainPtr domain)
{
    std::string mac_addr;
    std::unique_ptr<char, decltype(free)*> desc{virDomainGetXMLDesc(domain, 0), free};

    QXmlStreamReader reader(desc.get());

    while (!reader.atEnd())
    {
        reader.readNext();

        if (reader.name() == "mac")
        {
            mac_addr = reader.attributes().value("address").toString().toStdString();
            break;
        }
    }

    return mac_addr;
}

auto instance_ip_for(virConnectPtr connection, const std::string& mac_addr)
{
    mp::optional<mp::IPAddress> ip_address;

    mp::LibVirtVirtualMachine::NetworkUPtr network{virNetworkLookupByName(connection, "default"), virNetworkFree};

    virNetworkDHCPLeasePtr* leases = nullptr;
    auto nleases = virNetworkGetDHCPLeases(network.get(), mac_addr.c_str(), &leases, 0);

    auto leases_deleter = [&nleases](virNetworkDHCPLeasePtr* leases) {
        for (auto i = 0; i < nleases; ++i)
        {
            virNetworkDHCPLeaseFree(leases[i]);
        }
        free(leases);
    };

    std::unique_ptr<virNetworkDHCPLeasePtr, decltype(leases_deleter)> leases_ptr{leases, leases_deleter};
    if (nleases > 0)
    {
        ip_address.emplace(leases[0]->ipaddr);
    }

    return ip_address;
}

void parsed_memory_values_for(const std::string& mem_size, std::string& memory, std::string& mem_unit)
{
    auto mem{QString::fromStdString(mem_size)};
    QString unit;

    for (const auto& c : mem)
    {
        if (c.isDigit())
            memory.append(1, c.toLatin1());
        else if (c.isLetter())
            unit.append(c);
    }

    if (unit.isEmpty())
        mem_unit = "b";
    else if (unit.startsWith("K"))
        mem_unit = "KiB";
    else if (unit.startsWith("M"))
        mem_unit = "MiB";
    else if (unit.startsWith("G"))
        mem_unit = "GiB";
}

auto host_architecture_for(virConnectPtr connection)
{
    std::string arch;
    std::unique_ptr<char, decltype(free)*> capabilities{virConnectGetCapabilities(connection), free};

    QXmlStreamReader reader(capabilities.get());

    while (!reader.atEnd())
    {
        reader.readNext();

        if (reader.name() == "arch")
        {
            arch = reader.readElementText().toStdString();
            break;
        }
    }

    return arch;
}

auto generate_xml_config_for(const mp::VirtualMachineDescription& desc, const std::string& bridge_name,
                             const std::string& arch)
{
    std::string memory, mem_unit;
    parsed_memory_values_for(desc.mem_size, memory, mem_unit);

    auto qemu_path = fmt::format("/usr/bin/qemu-system-{}", arch);

    auto snap = qgetenv("SNAP");
    if (!snap.isEmpty())
    {
        auto snap_path = QDir(snap);
        snap_path.cd("../current");
        qemu_path = fmt::format("{}{}", snap_path.path().toStdString(), qemu_path);
    }

    return fmt::format(
        "<domain type=\'kvm\'>\n"
        "  <name>{}</name>\n"
        "  <memory unit=\'{}\'>{}</memory>\n"
        "  <currentMemory unit=\'{}\'>{}</currentMemory>\n"
        "  <vcpu placement=\'static\'>{}</vcpu>\n"
        "  <resource>\n"
        "    <partition>/machine</partition>\n"
        "  </resource>\n"
        "  <os>\n"
        "    <type arch=\'{}\'>hvm</type>\n"
        "    <boot dev=\'hd\'/>\n"
        "  </os>\n"
        "  <features>\n"
        "    <acpi/>\n"
        "    <apic/>\n"
        "    <vmport state=\'off\'/>\n"
        "  </features>\n"
        "  <cpu mode=\'host-passthrough\'>\n"
        "  </cpu>\n"
        "  <devices>\n"
        "    <emulator>{}</emulator>\n"
        "    <disk type=\'file\' device=\'disk\'>\n"
        "      <driver name=\'qemu\' type=\'qcow2\'/>\n"
        "      <source file=\'{}\'/>\n"
        "      <backingStore/>\n"
        "      <target dev=\'vda\' bus=\'virtio\'/>\n"
        "      <alias name=\'virtio-disk0\'/>\n"
        "    </disk>\n"
        "    <disk type=\'file\' device=\'disk\'>\n"
        "      <driver name=\'qemu\' type=\'raw\'/>\n"
        "      <source file=\'{}\'/>\n"
        "      <backingStore/>\n"
        "      <target dev=\'vdb\' bus=\'virtio\'/>\n"
        "      <alias name=\'virtio-disk1\'/>\n"
        "    </disk>\n"
        "    <interface type=\'bridge\'>\n"
        "      <mac address=\'{}\'/>\n"
        "      <source bridge=\'{}\'/>\n"
        "      <target dev=\'vnet0\'/>\n"
        "      <model type=\'virtio\'/>\n"
        "      <alias name=\'net0\'/>\n"
        "    </interface>\n"
        "    <serial type=\'pty\'>\n"
        "      <source path=\'/dev/pts/2\'/>\n"
        "      <target port=\"0\"/>\n"
        "    </serial>\n"
        "    <video>\n"
        "      <model type=\'qxl\' ram=\'65536\' vram=\'65536\' vgamem=\'16384\' heads=\'1\' primary=\'yes\'/>\n"
        "      <alias name=\'video0\'/>\n"
        "    </video>\n"
        "  </devices>\n"
        "</domain>",
        desc.vm_name, mem_unit, memory, mem_unit, memory, desc.num_cores, arch, qemu_path,
        desc.image.image_path.toStdString(), desc.cloud_init_iso.toStdString(), desc.mac_addr, bridge_name);
}

auto domain_definition_for(virConnectPtr connection, const mp::VirtualMachineDescription& desc,
                           const std::string& bridge_name)
{
    virSetErrorFunc(nullptr, libvirt_error_handler);

    mp::LibVirtVirtualMachine::DomainUPtr domain{virDomainLookupByName(connection, desc.vm_name.c_str()),
                                                 virDomainFree};

    virFreeError(libvirt_error);
    libvirt_error = nullptr;

    if (domain == nullptr)
        domain = mp::LibVirtVirtualMachine::DomainUPtr{
            virDomainDefineXML(connection,
                               generate_xml_config_for(desc, bridge_name, host_architecture_for(connection)).c_str()),
            virDomainFree};

    if (domain == nullptr)
        throw std::runtime_error("Error getting domain definition");

    return domain;
}

auto domain_state_for(virDomainPtr domain)
{
    auto state{0};

    if (virDomainHasManagedSaveImage(domain, 0) == 1)
        return mp::VirtualMachine::State::suspended;

    virDomainGetState(domain, &state, nullptr, 0);

    return (state == VIR_DOMAIN_RUNNING) ? mp::VirtualMachine::State::running : mp::VirtualMachine::State::off;
}
} // namespace

mp::LibVirtVirtualMachine::LibVirtVirtualMachine(const mp::VirtualMachineDescription& desc, virConnectPtr connection,
                                                 const std::string& bridge_name, mp::VMStatusMonitor& monitor)
    : VirtualMachine{desc.key_provider, desc.vm_name},
      connection{connection},
      domain{domain_definition_for(connection, desc, bridge_name)},
      mac_addr{instance_mac_addr_for(domain.get())},
      username{desc.ssh_username},
      ip{instance_ip_for(connection, mac_addr)},
      monitor{&monitor}
{
    state = domain_state_for(domain.get());
}

mp::LibVirtVirtualMachine::~LibVirtVirtualMachine()
{
    update_suspend_status = false;

    if (state == State::running)
        suspend();
}

void mp::LibVirtVirtualMachine::start()
{
    if (state == State::running)
        return;

    if (state == State::suspended)
        mpl::log(mpl::Level::info, vm_name, fmt::format("Resuming from a suspended state"));

    state = State::starting;
    update_state();

    if (virDomainCreate(domain.get()) == -1)
        throw std::runtime_error(virGetLastErrorMessage());

    monitor->on_resume();
}

void mp::LibVirtVirtualMachine::stop()
{
    shutdown();
}

void mp::LibVirtVirtualMachine::shutdown()
{
    if (state == State::running || state == State::delayed_shutdown)
    {
        virDomainShutdown(domain.get());
        state = State::off;
        update_state();
    }
    else if (state == State::suspended)
    {
        mpl::log(mpl::Level::info, vm_name, fmt::format("Ignoring shutdown issued while suspended"));
    }

    monitor->on_shutdown();
}

void mp::LibVirtVirtualMachine::suspend()
{
    if (state == State::running || state == State::delayed_shutdown)
    {
        if (virDomainManagedSave(domain.get(), 0) < 0)
            throw std::runtime_error(virGetLastErrorMessage());

        if (update_suspend_status)
        {
            state = State::suspended;
            update_state();
        }
    }
    else if (state == State::off)
    {
        mpl::log(mpl::Level::info, vm_name, fmt::format("Ignoring suspend issued while stopped"));
    }

    monitor->on_suspend();
}

mp::VirtualMachine::State mp::LibVirtVirtualMachine::current_state()
{
    return state;
}

int mp::LibVirtVirtualMachine::ssh_port()
{
    return 22;
}

std::string mp::LibVirtVirtualMachine::ssh_hostname()
{
    auto action = [this] {
        auto result = instance_ip_for(connection, mac_addr);
        if (result)
        {
            ip.emplace(result.value());
            return mp::utils::TimeoutAction::done;
        }
        else
        {
            return mp::utils::TimeoutAction::retry;
        }
    };
    auto on_timeout = [] { return std::runtime_error("failed to determine IP address"); };
    mp::utils::try_action_for(on_timeout, std::chrono::minutes(2), action);

    return ip.value().as_string();
}

std::string mp::LibVirtVirtualMachine::ssh_username()
{
    return username;
}

std::string mp::LibVirtVirtualMachine::ipv4()
{
    if (!ip)
    {
        auto result = instance_ip_for(connection, mac_addr);
        if (result)
            ip.emplace(result.value());
        else
            return "UNKNOWN";
    }

    return ip.value().as_string();
}

std::string mp::LibVirtVirtualMachine::ipv6()
{
    return {};
}

void mp::LibVirtVirtualMachine::wait_until_ssh_up(std::chrono::milliseconds timeout)
{
    mp::utils::wait_until_ssh_up(this, timeout);
}

void mp::LibVirtVirtualMachine::update_state()
{
    monitor->persist_state_for(vm_name);
}
