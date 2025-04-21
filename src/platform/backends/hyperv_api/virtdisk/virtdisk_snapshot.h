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

#ifndef MULTIPASS_HYPERV_API_VIRTDISK_SNAPSHOT_H
#define MULTIPASS_HYPERV_API_VIRTDISK_SNAPSHOT_H

#include <shared/base_snapshot.h>

#include <QString>

namespace multipass::hyperv::virtdisk
{

class VirtDiskSnapshot : public BaseSnapshot
{
    VirtDiskSnapshot(const std::string& name,
                     const std::string& comment,
                     const std::string& cloud_init_instance_id,
                     std::shared_ptr<Snapshot> parent,
                     const VMSpecs& specs,
                     const VirtualMachine& vm);
    VirtDiskSnapshot(const QString& filename, VirtualMachine& vm, const VirtualMachineDescription& desc);

protected:
    void capture_impl() override;
    void erase_impl() override;
    void apply_impl() override;
};

} // namespace multipass::hyperv::virtdisk

#endif
