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

#include <hyperv_api/virtdisk/virtdisk_snapshot.h>

#include <multipass/virtual_machine.h>
#include <multipass/virtual_machine_description.h>

namespace multipass::hyperv::virtdisk
{

// on delete: merge to parent

VirtDiskSnapshot::VirtDiskSnapshot(const std::string& name,
                                   const std::string& comment,
                                   const std::string& cloud_init_instance_id,
                                   std::shared_ptr<Snapshot> parent,
                                   const VMSpecs& specs,
                                   const VirtualMachine& vm)
    : BaseSnapshot(name, comment, cloud_init_instance_id, std::move(parent), specs, vm)
{
}

VirtDiskSnapshot::VirtDiskSnapshot(const QString& filename, VirtualMachine& vm, const VirtualMachineDescription& desc)
    : BaseSnapshot(filename, vm, desc)
{
    // std::filesystem::path target_file_path{filename.toStdString()};
    // std::filesystem::path vm_disk_path{desc.image.image_path.toStdString()};
}

// Create a differencing disk.

void VirtDiskSnapshot::capture_impl()
{
}

void VirtDiskSnapshot::erase_impl()
{
}

void VirtDiskSnapshot::apply_impl()
{
}

} // namespace multipass::hyperv::virtdisk
