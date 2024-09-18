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

#include "libvirt_virtual_machine.h"

#include <multipass/exceptions/start_exception.h>
#include <multipass/exceptions/virtual_machine_state_exceptions.h>
#include <multipass/format.h>
#include <multipass/logging/log.h>
#include <multipass/memory_size.h>
#include <multipass/vm_status_monitor.h>

#include <shared/linux/backend_utils.h>
#include <shared/qemu_img_utils/qemu_img_utils.h>
#include <shared/shared_backend_utils.h>

#include <QXmlStreamReader>

namespace mp = multipass;
namespace mpl = multipass::logging;

namespace
{
auto instance_mac_addr_for(virDomainPtr domain, const mp::LibvirtWrapper::UPtr& libvirt_wrapper)
{
    std::string mac_addr;
    std::unique_ptr<char, decltype(free)*> desc{libvirt_wrapper->virDomainGetXMLDesc(domain, 0), free};

    QXmlStreamReader reader(desc.get());

    while (!reader.atEnd())
    {
        reader.readNext();

        if (reader.name() == QStringLiteral("mac"))
        {
            mac_addr = reader.attributes().value("address").toString().toStdString();
            break;
        }
    }

    return mac_addr;
}

auto instance_ip_for(const std::string& mac_addr, const mp::LibvirtWrapper::UPtr& libvirt_wrapper)
{
    std::optional<mp::IPAddress> ip_address;

    mp::LibVirtVirtualMachine::ConnectionUPtr connection{nullptr, nullptr};
    try
    {
        connection = mp::LibVirtVirtualMachine::open_libvirt_connection(libvirt_wrapper);
    }
    catch (const std::exception&)
    {
        return ip_address;
    }

    mp::LibVirtVirtualMachine::NetworkUPtr network{libvirt_wrapper->virNetworkLookupByName(connection.get(), "default"),
                                                   libvirt_wrapper->virNetworkFree};

    virNetworkDHCPLeasePtr* leases = nullptr;
    auto nleases = libvirt_wrapper->virNetworkGetDHCPLeases(network.get(), mac_addr.c_str(), &leases, 0);

    auto leases_deleter = [&nleases, &libvirt_wrapper](virNetworkDHCPLeasePtr* leases) {
        for (auto i = 0; i < nleases; ++i)
        {
            libvirt_wrapper->virNetworkDHCPLeaseFree(leases[i]);
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

auto host_architecture_for(virConnectPtr connection, const mp::LibvirtWrapper::UPtr& libvirt_wrapper)
{
    std::string arch;
    std::unique_ptr<char, decltype(free)*> capabilities{libvirt_wrapper->virConnectGetCapabilities(connection), free};

    QXmlStreamReader reader(capabilities.get());

    while (!reader.atEnd())
    {
        reader.readNext();

        if (reader.name() == QStringLiteral("arch"))
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
    static constexpr auto mem_unit = "k"; // see https://libvirt.org/formatdomain.html#elementsMemoryAllocation
    const auto memory = desc.mem_size.in_kilobytes(); /* floored here, but then "[...] the value will be rounded up to
    the nearest kibibyte by libvirt, and may be further rounded to the granularity supported by the hypervisor [...]" */

    auto qemu_path = fmt::format("/usr/bin/qemu-system-{}", arch);

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
        "      <driver name=\'qemu\' type=\'qcow2\' discard=\'unmap\'/>\n"
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
        desc.image.image_path.toStdString(), desc.cloud_init_iso.toStdString(), desc.default_mac_address, bridge_name);
}

auto domain_by_name_for(const std::string& vm_name, virConnectPtr connection,
                        const mp::LibvirtWrapper::UPtr& libvirt_wrapper)
{
    mp::LibVirtVirtualMachine::DomainUPtr domain{libvirt_wrapper->virDomainLookupByName(connection, vm_name.c_str()),
                                                 libvirt_wrapper->virDomainFree};

    return domain;
}

auto domain_by_definition_for(const mp::VirtualMachineDescription& desc, const std::string& bridge_name,
                              virConnectPtr connection, const mp::LibvirtWrapper::UPtr& libvirt_wrapper)
{
    mp::LibVirtVirtualMachine::DomainUPtr domain{
        libvirt_wrapper->virDomainDefineXML(
            connection,
            generate_xml_config_for(desc, bridge_name, host_architecture_for(connection, libvirt_wrapper)).c_str()),
        libvirt_wrapper->virDomainFree};

    return domain;
}

auto refresh_instance_state_for_domain(virDomainPtr domain, const mp::VirtualMachine::State& current_instance_state,
                                       const mp::LibvirtWrapper::UPtr& libvirt_wrapper)
{
    auto domain_state{0};

    if (!domain || libvirt_wrapper->virDomainGetState(domain, &domain_state, nullptr, 0) == -1 ||
        domain_state == VIR_DOMAIN_NOSTATE)
        return mp::VirtualMachine::State::unknown;

    if (libvirt_wrapper->virDomainHasManagedSaveImage(domain, 0) == 1)
        return mp::VirtualMachine::State::suspended;

    // Most of these libvirt domain states don't have a Multipass instance state
    // analogue, so we'll treat them as "off".
    const auto domain_off_states = {VIR_DOMAIN_BLOCKED, VIR_DOMAIN_PAUSED,  VIR_DOMAIN_SHUTDOWN,
                                    VIR_DOMAIN_SHUTOFF, VIR_DOMAIN_CRASHED, VIR_DOMAIN_PMSUSPENDED};

    if (std::find(domain_off_states.begin(), domain_off_states.end(), domain_state) != domain_off_states.end())
        return mp::VirtualMachine::State::off;

    if (domain_state == VIR_DOMAIN_RUNNING && current_instance_state == mp::VirtualMachine::State::off)
        return mp::VirtualMachine::State::running;

    return current_instance_state;
}

bool domain_is_running(virDomainPtr domain, const mp::LibvirtWrapper::UPtr& libvirt_wrapper)
{
    auto domain_state{0};

    if (libvirt_wrapper->virDomainGetState(domain, &domain_state, nullptr, 0) == -1 ||
        domain_state != VIR_DOMAIN_RUNNING)
        return false;

    return true;
}

template <typename Updater, typename Integer>
void update_max_and_property(virDomainPtr domain_ptr, Updater* fun_ptr, Integer new_value, unsigned int max_flag,
                             const std::string& property_name)
{
    assert(domain_ptr);

    int twice = 0;
    unsigned int flags = VIR_DOMAIN_AFFECT_CONFIG | max_flag;
    do
    {
        if (fun_ptr(domain_ptr, new_value, flags) < 0)
            throw std::runtime_error(fmt::format("Could not update property: {}", property_name));

        flags &= ~max_flag;
    } while (!twice++); // first set the maximum, then actual
}

std::string management_ipv4_impl(std::optional<mp::IPAddress>& management_ip,
                                 const std::string& mac_addr,
                                 const mp::LibvirtWrapper::UPtr& libvirt_wrapper)
{
    if (!management_ip)
    {
        auto result = instance_ip_for(mac_addr, libvirt_wrapper);
        if (result)
            management_ip.emplace(result.value());
        else
            return "UNKNOWN";
    }

    return management_ip.value().as_string();
}
} // namespace

mp::LibVirtVirtualMachine::LibVirtVirtualMachine(const VirtualMachineDescription& desc,
                                                 const std::string& bridge_name,
                                                 VMStatusMonitor& monitor,
                                                 const LibvirtWrapper::UPtr& libvirt_wrapper,
                                                 const SSHKeyProvider& key_provider,
                                                 const Path& instance_dir)
    : BaseVirtualMachine{desc.vm_name, key_provider, instance_dir},
      username{desc.ssh_username},
      desc{desc},
      monitor{&monitor},
      bridge_name{bridge_name},
      libvirt_wrapper{libvirt_wrapper}
{
    try
    {
        initialize_domain_info(open_libvirt_connection(libvirt_wrapper).get());
    }
    catch (const std::exception&)
    {
        state = VirtualMachine::State::unknown;
    }
}

mp::LibVirtVirtualMachine::~LibVirtVirtualMachine()
{
    update_suspend_status = false;

    if (state == State::running)
        suspend();
}

void mp::LibVirtVirtualMachine::start()
{
    auto connection = open_libvirt_connection(libvirt_wrapper);
    DomainUPtr domain{nullptr, nullptr};

    if (state == VirtualMachine::State::unknown)
        domain = initialize_domain_info(connection.get());
    else
        domain = domain_by_name_for(vm_name, connection.get(), libvirt_wrapper);

    state = refresh_instance_state_for_domain(domain.get(), state, libvirt_wrapper);

    if (state == State::suspended)
        mpl::log(mpl::Level::info, vm_name, fmt::format("Resuming from a suspended state"));

    state = State::starting;
    update_state();

    if (libvirt_wrapper->virDomainCreate(domain.get()) == -1)
    {
        state = State::suspended;
        update_state();

        std::string error_string{libvirt_wrapper->virGetLastErrorMessage()};
        if (error_string.find("virtio-net-pci.rom: 0x80000 in != 0x40000") != std::string::npos)
        {
            error_string = fmt::format("Unable to start suspended instance due to incompatible save image.\n"
                                       "Please use the following steps to recover:\n"
                                       "  1. snap refresh multipass --channel core16/beta\n"
                                       "  2. multipass start {}\n"
                                       "  3. Save any data in the instance\n"
                                       "  4. multipass stop {}\n"
                                       "  5. snap refresh multipass --channel stable\n"
                                       "  6. multipass start {}\n",
                                       vm_name, vm_name, vm_name);
        }

        throw std::runtime_error(error_string);
    }

    monitor->on_resume();
}

void mp::LibVirtVirtualMachine::shutdown(ShutdownPolicy shutdown_policy)
{
    std::unique_lock<std::mutex> lock{state_mutex};
    auto domain = checked_vm_domain();

    state = refresh_instance_state_for_domain(domain.get(), state, libvirt_wrapper);

    try
    {
        check_state_for_shutdown(shutdown_policy);
    }
    catch (const VMStateIdempotentException& e)
    {
        mpl::log(mpl::Level::info, vm_name, e.what());
        return;
    }

    if (shutdown_policy == ShutdownPolicy::Poweroff) // TODO delete suspension state if it exists
    {
        mpl::log(mpl::Level::info, vm_name, "Forcing shutdown");

        libvirt_wrapper->virDomainDestroy(domain.get());

        if (state == State::starting || state == State::restarting)
        {
            libvirt_wrapper->virDomainDestroy(domain.get());
            state_wait.wait(lock, [this] { return shutdown_while_starting; });
        }
    }
    else
    {
        drop_ssh_session();

        if (libvirt_wrapper->virDomainShutdown(domain.get()) == -1)
        {
            auto warning_string{
                fmt::format("Cannot shutdown '{}': {}", vm_name, libvirt_wrapper->virGetLastErrorMessage())};
            mpl::log(mpl::Level::warning, vm_name, warning_string);
            throw std::runtime_error(warning_string);
        }
    }

    state = State::off;
    update_state();
    monitor->on_shutdown();
}

void mp::LibVirtVirtualMachine::suspend()
{
    auto domain = domain_by_name_for(vm_name, open_libvirt_connection(libvirt_wrapper).get(), libvirt_wrapper);
    state = refresh_instance_state_for_domain(domain.get(), state, libvirt_wrapper);
    if (state == State::running || state == State::delayed_shutdown)
    {
        drop_ssh_session();
        if (!domain || libvirt_wrapper->virDomainManagedSave(domain.get(), 0) < 0)
        {
            auto warning_string{
                fmt::format("Cannot suspend '{}': {}", vm_name, libvirt_wrapper->virGetLastErrorMessage())};
            mpl::log(mpl::Level::warning, vm_name, warning_string);
            throw std::runtime_error(warning_string);
        }

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
    try
    {
        auto connection = open_libvirt_connection(libvirt_wrapper);
        auto domain = domain_by_name_for(vm_name, connection.get(), libvirt_wrapper);
        if (!domain)
            initialize_domain_info(connection.get());

        state = refresh_instance_state_for_domain(domain.get(), state, libvirt_wrapper);
    }
    catch (const std::exception&)
    {
        state = VirtualMachine::State::unknown;
    }

    return state;
}

int mp::LibVirtVirtualMachine::ssh_port()
{
    return 22;
}

void mp::LibVirtVirtualMachine::ensure_vm_is_running()
{
    auto is_vm_running = [this] {
        auto domain = domain_by_name_for(vm_name, open_libvirt_connection(libvirt_wrapper).get(), libvirt_wrapper);
        return domain_is_running(domain.get(), libvirt_wrapper);
    };

    mp::backend::ensure_vm_is_running_for(this, is_vm_running, "Instance failed to start");
}

std::string mp::LibVirtVirtualMachine::ssh_hostname(std::chrono::milliseconds timeout)
{
    auto get_ip = [this]() -> std::optional<IPAddress> { return instance_ip_for(mac_addr, libvirt_wrapper); };

    return mp::backend::ip_address_for(this, get_ip, timeout);
}

std::string mp::LibVirtVirtualMachine::ssh_username()
{
    return username;
}

std::string mp::LibVirtVirtualMachine::management_ipv4()
{
    return management_ipv4_impl(management_ip, mac_addr, libvirt_wrapper);
}

std::string mp::LibVirtVirtualMachine::ipv6()
{
    return {};
}

void mp::LibVirtVirtualMachine::update_state()
{
    monitor->persist_state_for(vm_name, state);
}

mp::LibVirtVirtualMachine::DomainUPtr mp::LibVirtVirtualMachine::initialize_domain_info(virConnectPtr connection)
{
    auto domain = domain_by_name_for(vm_name, connection, libvirt_wrapper);

    if (!domain)
    {
        domain = domain_by_definition_for(desc, bridge_name, connection, libvirt_wrapper);
    }

    if (mac_addr.empty())
        mac_addr = instance_mac_addr_for(domain.get(), libvirt_wrapper);

    management_ipv4_impl(management_ip, mac_addr, libvirt_wrapper); // To set the IP.
    state = refresh_instance_state_for_domain(domain.get(), state, libvirt_wrapper);

    return domain;
}

mp::LibVirtVirtualMachine::DomainUPtr mp::LibVirtVirtualMachine::checked_vm_domain() const
{
    auto connection = open_libvirt_connection(libvirt_wrapper);
    assert(connection && "should have thrown otherwise");

    auto domain = domain_by_name_for(vm_name, connection.get(), libvirt_wrapper);
    if (!domain)
        throw std::runtime_error{
            fmt::format("Could not obtain libvirt domain: {}", libvirt_wrapper->virGetLastErrorMessage())};

    return domain;
}

mp::LibVirtVirtualMachine::ConnectionUPtr
mp::LibVirtVirtualMachine::open_libvirt_connection(const mp::LibvirtWrapper::UPtr& libvirt_wrapper)
{
    if (!libvirt_wrapper)
        throw std::runtime_error("The libvirt library is not loaded. Please ensure libvirt is installed and running.");

    mp::LibVirtVirtualMachine::ConnectionUPtr connection{libvirt_wrapper->virConnectOpen("qemu:///system"),
                                                         libvirt_wrapper->virConnectClose};
    if (!connection)
    {
        throw std::runtime_error(
            fmt::format("Cannot connect to libvirtd: {}\nPlease ensure libvirt is installed and running.",
                        libvirt_wrapper->virGetLastErrorMessage()));
    }

    return connection;
}

void mp::LibVirtVirtualMachine::update_cpus(int num_cores)
{
    assert(num_cores > 0);
    update_max_and_property(checked_vm_domain().get(), libvirt_wrapper->virDomainSetVcpusFlags, num_cores,
                            VIR_DOMAIN_VCPU_MAXIMUM, "CPUs");

    desc.num_cores = num_cores;
}

void mp::LibVirtVirtualMachine::resize_memory(const MemorySize& new_size)
{
    auto new_size_kb = new_size.in_kilobytes(); /* floored here */
    update_max_and_property(checked_vm_domain().get(), libvirt_wrapper->virDomainSetMemoryFlags, new_size_kb,
                            VIR_DOMAIN_MEM_MAXIMUM, "memory");

    desc.mem_size = new_size;
}

void mp::LibVirtVirtualMachine::resize_disk(const MemorySize& new_size)
{
    assert(new_size > desc.disk_space);

    mp::backend::resize_instance_image(new_size, desc.image.image_path);
    desc.disk_space = new_size;
}
