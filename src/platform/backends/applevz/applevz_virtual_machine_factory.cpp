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

#include <applevz/applevz_utils.h>
#include <applevz/applevz_virtual_machine.h>
#include <applevz/applevz_virtual_machine_factory.h>
#include <multipass/utils/qemu_img_utils.h>

namespace mp = multipass;

namespace multipass::applevz
{
AppleVZVirtualMachineFactory::AppleVZVirtualMachineFactory(const Path& data_dir)
    : BaseVirtualMachineFactory(
          MP_UTILS.derive_instances_dir(data_dir, get_backend_directory_name(), instances_subdir))
{
}

VirtualMachine::UPtr AppleVZVirtualMachineFactory::create_virtual_machine(
    const VirtualMachineDescription& desc,
    const SSHKeyProvider& key_provider,
    VMStatusMonitor& monitor)
{
    return std::make_unique<mp::applevz::AppleVZVirtualMachine>(
        desc,
        monitor,
        key_provider,
        get_instance_directory(desc.vm_name));
}

VMImage AppleVZVirtualMachineFactory::prepare_source_image(const VMImage& source_image)
{
    VMImage image{source_image};
    image.image_path = convert_to_supported_format(source_image.image_path);
    return image;
}

void AppleVZVirtualMachineFactory::prepare_instance_image(const VMImage& instance_image,
                                                          const VirtualMachineDescription& desc)
{
    applevz::resize_image(desc.disk_space, instance_image.image_path);
}

void AppleVZVirtualMachineFactory::hypervisor_health_check()
{
    if (!MP_APPLEVZ.is_supported())
    {
        throw std::runtime_error("Virtualization is not supported on this system.");
    }
}

void AppleVZVirtualMachineFactory::remove_resources_for_impl(const std::string& name)
{
}

VirtualMachine::UPtr AppleVZVirtualMachineFactory::clone_vm_impl(
    const std::string& /*source_vm_name*/,
    const multipass::VMSpecs& /*src_vm_specs*/,
    const VirtualMachineDescription& desc,
    VMStatusMonitor& monitor,
    const SSHKeyProvider& key_provider)
{
    return std::make_unique<mp::applevz::AppleVZVirtualMachine>(
        desc,
        monitor,
        key_provider,
        get_instance_directory(desc.vm_name));
}
} // namespace multipass::applevz
