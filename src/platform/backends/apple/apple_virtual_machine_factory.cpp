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

#include <apple/apple_virtual_machine_factory.h>

namespace multipass::apple
{
AppleVirtualMachineFactory::AppleVirtualMachineFactory(const Path& data_dir)
    : BaseVirtualMachineFactory(
          MP_UTILS.derive_instances_dir(data_dir, get_backend_directory_name(), instances_subdir))
{
}

VirtualMachine::UPtr AppleVirtualMachineFactory::create_virtual_machine(
    const VirtualMachineDescription& desc,
    const SSHKeyProvider& key_provider,
    VMStatusMonitor& monitor)
{
    return nullptr;
}

void AppleVirtualMachineFactory::remove_resources_for_impl(const std::string& name)
{
}

VMImage AppleVirtualMachineFactory::prepare_source_image(const VMImage& source_image)
{
    return VMImage{};
}

void AppleVirtualMachineFactory::prepare_instance_image(const VMImage& instance_image,
                                                        const VirtualMachineDescription& desc)
{
}

void AppleVirtualMachineFactory::hypervisor_health_check()
{
}
} // namespace multipass::apple
