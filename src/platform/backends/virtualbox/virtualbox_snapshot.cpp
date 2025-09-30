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

#include "virtualbox_snapshot.h"
#include "virtualbox_virtual_machine.h"

#include <multipass/utils.h>

namespace mp = multipass;
namespace mpu = multipass::utils;

namespace
{
bool snapshot_exists(const QString& vm_name, const QString& snapshot_id)
{
    return mpu::process_log_on_error("VBoxManage",
                                     {"snapshot", vm_name, "showvminfo", snapshot_id},
                                     "Could not find snapshot: {}",
                                     vm_name);
}

void require_unique_id(const QString& vm_name, const QString& snapshot_id)
{
    if (snapshot_exists(vm_name, snapshot_id))
        throw std::runtime_error{
            fmt::format("A snapshot with ID {} already exists for {} in VirtualBox",
                        snapshot_id,
                        vm_name)};
}
} // namespace

mp::VirtualBoxSnapshot::VirtualBoxSnapshot(const std::string& name,
                                           const std::string& comment,
                                           const std::string& cloud_init_instance_id,
                                           std::shared_ptr<Snapshot> parent,
                                           const QString& vm_name,
                                           const VMSpecs& specs,
                                           VirtualBoxVirtualMachine& vm)
    : BaseSnapshot{name, comment, cloud_init_instance_id, std::move(parent), specs, vm},
      vm_name{vm_name}
{
}

mp::VirtualBoxSnapshot::VirtualBoxSnapshot(const QString& filename,
                                           VirtualBoxVirtualMachine& vm,
                                           const VirtualMachineDescription& desc)
    : BaseSnapshot{filename, vm, desc}, vm_name{QString::fromStdString(desc.vm_name)}
{
}

void multipass::VirtualBoxSnapshot::capture_impl()
{
    const auto& id = get_id();
    require_unique_id(vm_name, id);

    const auto description_arg =
        QString{"--description=%1: %2"}.arg(get_name().c_str(), get_comment().c_str());
    mpu::process_throw_on_error("VBoxManage",
                                {"snapshot", vm_name, "take", id, description_arg},
                                "Could not take snapshot: {}",
                                vm_name);
}

void multipass::VirtualBoxSnapshot::erase_impl()
{
    const auto& id = get_id();
    if (snapshot_exists(vm_name, id))
        mpu::process_throw_on_error("VBoxManage",
                                    {"snapshot", vm_name, "delete", get_id()},
                                    "Could not delete snapshot: {}",
                                    vm_name);
    else
        mpl::warn(
            vm_name.toStdString(),
            fmt::format("Could not find underlying VirtualBox snapshot for \"{}\". Ignoring...",
                        get_name()));
}

void multipass::VirtualBoxSnapshot::apply_impl()
{
    mpu::process_throw_on_error("VBoxManage",
                                {"snapshot", vm_name, "restore", get_id()},
                                "Could not restore snapshot: {}",
                                vm_name);
}
