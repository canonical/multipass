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

#ifndef MULTIPASS_HYPERVSNAPSHOT_H
#define MULTIPASS_HYPERVSNAPSHOT_H

#include <shared/base_snapshot.h>

#include <QString>

namespace multipass
{
class HyperVVirtualMachine;
class PowerShell;

class HyperVSnapshot : public BaseSnapshot
{
public:
    HyperVSnapshot(const std::string& name,
                   const std::string& comment,
                   const std::string& cloud_init_instance_id,
                   const VMSpecs& specs,
                   std::shared_ptr<Snapshot> parent,
                   const QString& vm_name,
                   HyperVVirtualMachine& vm,
                   PowerShell& power_shell);
    HyperVSnapshot(const QString& filename,
                   HyperVVirtualMachine& vm,
                   const VirtualMachineDescription& desc,
                   PowerShell& power_shell);

protected:
    void capture_impl() override;
    void erase_impl() override;
    void apply_impl() override;

private:
    const QString quoted_id;
    const QString vm_name;
    PowerShell& power_shell;
};

} // namespace multipass

#endif // MULTIPASS_HYPERVSNAPSHOT_H
