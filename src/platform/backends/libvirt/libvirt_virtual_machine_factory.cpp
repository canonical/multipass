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
#include <multipass/utils.h>
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

auto generate_libvirt_bridge_xml_config(const std::string& bridge_name)
{
    auto subnet = mp::backend::generate_random_subnet();

    return fmt::format("<network>\n"
                       "  <name>default</name>\n"
                       "  <bridge name=\"{}\"/>\n"
                       "  <forward/>\n"
                       "  <ip address=\"{}.1\" netmask=\"255.255.255.0\">\n"
                       "    <dhcp>\n"
                       "      <range start=\"{}.2\" end=\"{}.254\"/>\n"
                       "    </dhcp>\n"
                       "  </ip>\n"
                       "</network>",
                       bridge_name, subnet, subnet, subnet);
}

std::string enable_libvirt_network(virConnectPtr connection)
{
    mp::LibVirtVirtualMachine::NetworkUPtr network{virNetworkLookupByName(connection, "default"), virNetworkFree};
    std::string bridge_name;

    if (network == nullptr)
    {
        bridge_name = mp::backend::generate_virtual_bridge_name("mpvirtbr");
        network = mp::LibVirtVirtualMachine::NetworkUPtr{
            virNetworkDefineXML(connection, generate_libvirt_bridge_xml_config(bridge_name).c_str()), virNetworkFree};
        virNetworkSetAutostart(network.get(), 1);
    }
    else
    {
        bridge_name = virNetworkGetBridgeName(network.get());
    }

    if (virNetworkIsActive(network.get()) == 0)
    {
        virNetworkCreate(network.get());
    }

    return bridge_name;
}
}

mp::LibVirtVirtualMachineFactory::LibVirtVirtualMachineFactory(const mp::Path& data_dir)
    : connection{connect_to_libvirt_daemon()}, bridge_name{enable_libvirt_network(connection.get())}
{
}

mp::VirtualMachine::UPtr mp::LibVirtVirtualMachineFactory::create_virtual_machine(const VirtualMachineDescription& desc,
                                                                                  VMStatusMonitor& monitor)
{
    return std::make_unique<mp::LibVirtVirtualMachine>(desc, connection.get(), bridge_name, monitor);
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
    VMImage image{source_image};

    if (mp::backend::image_format_for(source_image.image_path) == "raw")
    {
        auto qcow2_path{mp::Path(source_image.image_path).append(".qcow2")};
        mp::utils::run_cmd_for_status(
            "qemu-img", {QStringLiteral("convert"), "-p", "-O", "qcow2", source_image.image_path, qcow2_path});
        image.image_path = qcow2_path;
    }

    return image;
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
