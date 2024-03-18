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

#include "libvirt_virtual_machine_factory.h"
#include "libvirt_virtual_machine.h"

#include <multipass/logging/log.h>
#include <multipass/utils.h>
#include <multipass/virtual_machine_description.h>
#include <shared/linux/backend_utils.h>
#include <shared/qemu_img_utils/qemu_img_utils.h>

#include <multipass/format.h>

namespace mp = multipass;
namespace mpl = multipass::logging;

namespace
{
constexpr auto multipass_bridge_name = "mpvirtbr0";
constexpr auto logging_category = "libvirt factory";

auto generate_libvirt_bridge_xml_config(const mp::Path& data_dir, const std::string& bridge_name)
{
    auto network_dir = MP_UTILS.make_dir(QDir(data_dir), "network");
    auto subnet = MP_BACKEND.get_subnet(network_dir, QString::fromStdString(bridge_name));

    return fmt::format("<network>\n"
                       "  <name>default</name>\n"
                       "  <bridge name=\"{}\"/>\n"
                       "  <domain name=\"multipass\" localOnly=\"yes\"/>\n"
                       "  <forward/>\n"
                       "  <ip address=\"{}.1\" netmask=\"255.255.255.0\">\n"
                       "    <dhcp>\n"
                       "      <range start=\"{}.2\" end=\"{}.254\"/>\n"
                       "    </dhcp>\n"
                       "  </ip>\n"
                       "</network>",
                       bridge_name, subnet, subnet, subnet);
}

std::string enable_libvirt_network(const mp::Path& data_dir, const mp::LibvirtWrapper::UPtr& libvirt_wrapper)
{
    mp::LibVirtVirtualMachine::ConnectionUPtr connection{nullptr, nullptr};
    try
    {
        connection = mp::LibVirtVirtualMachine::open_libvirt_connection(libvirt_wrapper);
    }
    catch (const std::exception&)
    {
        return {};
    }

    mp::LibVirtVirtualMachine::NetworkUPtr network{libvirt_wrapper->virNetworkLookupByName(connection.get(), "default"),
                                                   libvirt_wrapper->virNetworkFree};
    std::string bridge_name;

    if (network == nullptr)
    {
        bridge_name = multipass_bridge_name;
        network = mp::LibVirtVirtualMachine::NetworkUPtr{
            libvirt_wrapper->virNetworkCreateXML(connection.get(),
                                                 generate_libvirt_bridge_xml_config(data_dir, bridge_name).c_str()),
            libvirt_wrapper->virNetworkFree};
    }
    else
    {
        auto libvirt_bridge = libvirt_wrapper->virNetworkGetBridgeName(network.get());
        bridge_name = libvirt_bridge;
        free(libvirt_bridge);
    }

    if (libvirt_wrapper->virNetworkIsActive(network.get()) == 0)
    {
        libvirt_wrapper->virNetworkCreate(network.get());
    }

    return bridge_name;
}

auto make_libvirt_wrapper(const std::string& libvirt_object_path)
{
    try
    {
        return std::make_unique<mp::LibvirtWrapper>(libvirt_object_path);
    }
    catch (const mp::BaseLibvirtException& e)
    {
        mpl::log(mpl::Level::warning, logging_category, e.what());
        return mp::LibvirtWrapper::UPtr(nullptr);
    }
}
} // namespace

mp::LibVirtVirtualMachineFactory::LibVirtVirtualMachineFactory(const mp::Path& data_dir,
                                                               const std::string& libvirt_object_path)
    : BaseVirtualMachineFactory(
          MP_UTILS.derive_instances_dir(data_dir, get_backend_directory_name(), instances_subdir)),
      libvirt_wrapper{make_libvirt_wrapper(libvirt_object_path)},
      data_dir{data_dir},
      bridge_name{enable_libvirt_network(data_dir, libvirt_wrapper)},
      libvirt_object_path{libvirt_object_path}
{
}

mp::LibVirtVirtualMachineFactory::LibVirtVirtualMachineFactory(const mp::Path& data_dir)
    : LibVirtVirtualMachineFactory(data_dir, "libvirt.so.0")
{
}

mp::VirtualMachine::UPtr mp::LibVirtVirtualMachineFactory::create_virtual_machine(const VirtualMachineDescription& desc,
                                                                                  const SSHKeyProvider& key_provider,
                                                                                  VMStatusMonitor& monitor)
{
    if (bridge_name.empty())
        bridge_name = enable_libvirt_network(data_dir, libvirt_wrapper);

    return std::make_unique<mp::LibVirtVirtualMachine>(desc,
                                                       bridge_name,
                                                       monitor,
                                                       libvirt_wrapper,
                                                       key_provider,
                                                       get_instance_directory(desc.vm_name));
}

mp::LibVirtVirtualMachineFactory::~LibVirtVirtualMachineFactory()
{
    if (bridge_name == multipass_bridge_name)
    {
        auto connection = LibVirtVirtualMachine::open_libvirt_connection(libvirt_wrapper);
        mp::LibVirtVirtualMachine::NetworkUPtr network{
            libvirt_wrapper->virNetworkLookupByName(connection.get(), "default"), libvirt_wrapper->virNetworkFree};

        libvirt_wrapper->virNetworkDestroy(network.get());
    }
}

void mp::LibVirtVirtualMachineFactory::remove_resources_for_impl(const std::string& name)
{
    auto connection = LibVirtVirtualMachine::open_libvirt_connection(libvirt_wrapper);

    libvirt_wrapper->virDomainUndefine(libvirt_wrapper->virDomainLookupByName(connection.get(), name.c_str()));
}

mp::VMImage mp::LibVirtVirtualMachineFactory::prepare_source_image(const VMImage& source_image)
{
    VMImage image{source_image};
    image.image_path = mp::backend::convert_to_qcow_if_necessary(source_image.image_path);
    return image;
}

void mp::LibVirtVirtualMachineFactory::prepare_instance_image(const VMImage& instance_image,
                                                              const VirtualMachineDescription& desc)
{
    mp::backend::resize_instance_image(desc.disk_space, instance_image.image_path);
}

void mp::LibVirtVirtualMachineFactory::hypervisor_health_check()
{
    MP_BACKEND.check_for_kvm_support();
    MP_BACKEND.check_if_kvm_is_in_use();

    if (!libvirt_wrapper)
        libvirt_wrapper = make_libvirt_wrapper(libvirt_object_path);

    LibVirtVirtualMachine::open_libvirt_connection(libvirt_wrapper);

    if (bridge_name.empty())
        bridge_name = enable_libvirt_network(data_dir, libvirt_wrapper);
}

QString mp::LibVirtVirtualMachineFactory::get_backend_version_string() const
{
    try
    {
        unsigned long libvirt_version;
        auto connection = LibVirtVirtualMachine::open_libvirt_connection(libvirt_wrapper);

        if (libvirt_wrapper->virConnectGetVersion(connection.get(), &libvirt_version) == 0 && libvirt_version != 0)
        {
            return QString("libvirt-%1.%2.%3")
                .arg(libvirt_version / 1000000)
                .arg(libvirt_version / 1000 % 1000)
                .arg(libvirt_version % 1000);
        }
    }
    catch (const std::exception&)
    {
        // Ignore
    }

    mpl::log(mpl::Level::error, logging_category, "Failed to determine libvirtd version.");
    return QString("libvirt-unknown");
}
