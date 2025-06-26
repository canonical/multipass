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

#pragma once

#include <shared/base_snapshot.h>

namespace multipass
{
class VirtualBoxVirtualMachine;
class VirtualMachineDescription;

class VirtualBoxSnapshot : public BaseSnapshot
{
public:
    VirtualBoxSnapshot(const std::string& name,
                       const std::string& comment,
                       const std::string& cloud_init_instance_id,
                       std::shared_ptr<Snapshot> parent,
                       const QString& vm_name,
                       const VMSpecs& specs,
                       VirtualBoxVirtualMachine& vm);
    VirtualBoxSnapshot(const QString& filename,
                       VirtualBoxVirtualMachine& vm,
                       const VirtualMachineDescription& desc);

protected:
    void capture_impl() override;
    void erase_impl() override;
    void apply_impl() override;

private:
    const QString vm_name;
};

} // namespace multipass
