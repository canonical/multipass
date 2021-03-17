/*
 * Copyright (C) 2021 Canonical, Ltd.
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

#include <multipass/cloud_init_iso.h>
#include <multipass/utils.h>
#include <multipass/virtual_machine_description.h>

namespace mp = multipass;
namespace mpu = multipass::utils;

void mp::BaseVirtualMachineFactory::configure(VirtualMachineDescription& vm_desc)
{
    auto instance_dir{mpu::base_dir(vm_desc.image.image_path)};
    const auto cloud_init_iso = instance_dir.filePath("cloud-init-config.iso");

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
