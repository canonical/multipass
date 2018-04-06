/*
 * Copyright (C) 2018 Canonical, Ltd.
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

#include "libvirt_virtual_machine_factory.h"
#include "libvirt_virtual_machine.h"

#include <multipass/backend_utils.h>
#include <multipass/virtual_machine_description.h>

#include <fmt/format.h>

namespace mp = multipass;

namespace
{
auto connect_to_libvirt_daemon()
{
    mp::LibVirtVirtualMachineFactory::ConnectionUPtr conn{virConnectOpen("qemu:///system"), virConnectClose};

    if (conn == nullptr)
        throw std::runtime_error("Cannot connect to libvirtd");

    return conn;
}

auto generate_libvirt_bridge_xml_config()
{
    auto subnet = mp::backend::generate_random_subnet();

    return fmt::format("<network>\n"
                       "  <name>default</name>\n"
                       "  <bridge name=\"mpbr0\"/>\n"
                       "  <forward/>\n"
                       "  <ip address=\"{}.1\" netmask=\"255.255.255.0\">\n"
                       "    <dhcp>\n"
                       "      <range start=\"{}.2\" end=\"{}.254\"/>\n"
                       "    </dhcp>\n"
                       "  </ip>\n"
                       "</network>",
                       subnet, subnet, subnet);
}

void enable_libvirt_network(virConnectPtr connection)
{
    mp::LibVirtVirtualMachine::NetworkUPtr network{virNetworkLookupByName(connection, "default"), virNetworkFree};

    if (network == nullptr)
    {
        network = mp::LibVirtVirtualMachine::NetworkUPtr{
            virNetworkCreateXML(connection, generate_libvirt_bridge_xml_config().c_str()), virNetworkFree};
    }
    else
    {
        if (virNetworkIsActive(network.get()) == 0)
        {
            virNetworkCreate(network.get());
        }
    }
}
}

mp::LibVirtVirtualMachineFactory::LibVirtVirtualMachineFactory(const mp::Path& data_dir)
    : connection{connect_to_libvirt_daemon()}
{
    enable_libvirt_network(connection.get());
}

mp::LibVirtVirtualMachineFactory::~LibVirtVirtualMachineFactory()
{
}

mp::VirtualMachine::UPtr mp::LibVirtVirtualMachineFactory::create_virtual_machine(const VirtualMachineDescription& desc,
                                                                                  VMStatusMonitor& monitor)
{
    return std::make_unique<mp::LibVirtVirtualMachine>(desc, connection.get(), monitor);
}

void mp::LibVirtVirtualMachineFactory::remove_resources_for(const std::string& name)
{
    virDomainUndefine(virDomainLookupByName(connection.get(), name.c_str()));
}

mp::FetchType mp::LibVirtVirtualMachineFactory::fetch_type()
{
    return mp::FetchType::ImageOnly;
}

mp::VMImage mp::LibVirtVirtualMachineFactory::prepare_source_image(const VMImage& source_image)
{
    return source_image;
}

void mp::LibVirtVirtualMachineFactory::prepare_instance_image(const VMImage& instance_image,
                                                              const VirtualMachineDescription& desc)
{
    mp::backend::resize_instance_image(desc.disk_space, instance_image.image_path);
}

void mp::LibVirtVirtualMachineFactory::configure(const std::string& name, YAML::Node& meta_config,
                                                 YAML::Node& user_config)
{
}

void mp::LibVirtVirtualMachineFactory::check_hypervisor_support()
{
    mp::backend::check_hypervisor_support();
}
