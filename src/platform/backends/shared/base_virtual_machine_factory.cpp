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

#include "base_virtual_machine_factory.h"
#include "multipass/platform.h"

#include <multipass/cloud_init_iso.h>
#include <multipass/constants.h>
#include <multipass/network_interface.h>
#include <multipass/network_interface_info.h>
#include <multipass/virtual_machine_description.h>
#include <multipass/yaml_node_utils.h>

namespace mp = multipass;
namespace mpu = multipass::utils;

const mp::Path mp::BaseVirtualMachineFactory::instances_subdir = "vault/instances";

mp::BaseVirtualMachineFactory::BaseVirtualMachineFactory(const Path& instances_dir) : instances_dir{instances_dir} {};

void mp::BaseVirtualMachineFactory::configure(VirtualMachineDescription& vm_desc)
{
    auto instance_dir{mpu::base_dir(vm_desc.image.image_path)};
    const auto cloud_init_iso = instance_dir.filePath(cloud_init_file_name);

    if (!QFile::exists(cloud_init_iso))
    {
        mp::CloudInitIso iso;
        iso.add_file("meta-data", mpu::emit_cloud_config(vm_desc.meta_data_config));
        iso.add_file("vendor-data", mpu::emit_cloud_config(vm_desc.vendor_data_config));
        iso.add_file("user-data", mpu::emit_cloud_config(vm_desc.user_data_config));
        if (!vm_desc.network_data_config.IsNull())
            iso.add_file("network-config", mpu::emit_cloud_config(vm_desc.network_data_config));

        iso.write_to(cloud_init_iso);
    }

    vm_desc.cloud_init_iso = cloud_init_iso;
}

void mp::BaseVirtualMachineFactory::prepare_networking(std::vector<NetworkInterface>& extra_interfaces)
{
    if (!extra_interfaces.empty())
    {
        auto host_nets = networks(); // expensive
        for (auto& net : extra_interfaces)
            prepare_interface(net, host_nets);
    }
}

void mp::BaseVirtualMachineFactory::prepare_interface(NetworkInterface& net,
                                                      std::vector<NetworkInterfaceInfo>& host_nets)
{
    const auto bridge_type = MP_PLATFORM.bridge_nomenclature();
    auto net_it = std::find_if(host_nets.cbegin(), host_nets.cend(),
                               [&net](const mp::NetworkInterfaceInfo& info) { return info.id == net.id; });

    if (net_it != host_nets.end() && net_it->type != bridge_type)
    {
        if (auto bridge = mpu::find_bridge_with(host_nets, net.id, bridge_type))
        {
            net.id = bridge->id;
        }
        else
        {
            net.id = create_bridge_with(*net_it);
            host_nets.push_back({net.id, bridge_type, "new bridge", {net_it->id}});
        }
    }
}
