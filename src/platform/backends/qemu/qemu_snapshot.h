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

#ifndef MULTIPASS_QEMU_SNAPSHOT_H
#define MULTIPASS_QEMU_SNAPSHOT_H

#include "qemu_virtual_machine.h"

#include <shared/base_snapshot.h>

#include <multipass/path.h>

namespace multipass
{
class QemuVirtualMachine;
class VirtualMachineDescription;

class QemuSnapshot : public BaseSnapshot
{
public:
    QemuSnapshot(const std::string& name,
                 const std::string& comment,
                 const std::string& cloud_init_instance_id,
                 std::shared_ptr<Snapshot> parent,
                 const VMSpecs& specs,
                 QemuVirtualMachine& vm,
                 VirtualMachineDescription& desc);
    QemuSnapshot(const QString& filename, QemuVirtualMachine& vm, VirtualMachineDescription& desc);
    QemuSnapshot(const QString& filename,
                 const VMSpecs& src_specs,
                 const VMSpecs& dest_specs,
                 const std::string& src_vm_name,
                 QemuVirtualMachine& vm,
                 VirtualMachineDescription& desc);

protected:
    void capture_impl() override;
    void erase_impl() override;
    void apply_impl() override;

private:
    VirtualMachineDescription& desc;
    const Path& image_path;
};

} // namespace multipass

#endif // MULTIPASS_QEMU_SNAPSHOT_H
