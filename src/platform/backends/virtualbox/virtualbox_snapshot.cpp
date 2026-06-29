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
bool snapshot_exists(const std::string& vm_name, const std::string& snapshot_id)
{
    const auto qvm_name = QString::fromStdString(vm_name);
    return mpu::process_log_on_error(
        "VBoxManage",
        {"snapshot", qvm_name, "showvminfo", QString::fromStdString(snapshot_id)},
        "Could not find snapshot: {}",
        qvm_name);
}

void require_unique_id(const std::string& vm_name, const std::string& snapshot_id)
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
                                           const std::string& vm_name,
                                           const VMSpecs& specs,
                                           VirtualBoxVirtualMachine& vm)
    : BaseSnapshot{name, comment, cloud_init_instance_id, std::move(parent), specs, vm},
      vm_name{vm_name}
{
}

mp::VirtualBoxSnapshot::VirtualBoxSnapshot(const std::filesystem::path& filename,
                                           VirtualBoxVirtualMachine& vm,
                                           const VirtualMachineDescription& desc)
    : BaseSnapshot{filename, vm, desc}, vm_name{desc.vm_name}
{
}

void multipass::VirtualBoxSnapshot::capture_impl()
{
    const auto& id = get_id();
    require_unique_id(vm_name, id);

    const auto description_arg = fmt::format("--description={}: {}", get_name(), get_comment());

    const auto qvm_name = QString::fromStdString(vm_name);
    mpu::process_throw_on_error("VBoxManage",
                                {"snapshot",
                                 qvm_name,
                                 "take",
                                 QString::fromStdString(id),
                                 QString::fromStdString(description_arg)},
                                "Could not take snapshot: {}",
                                qvm_name);
}

void multipass::VirtualBoxSnapshot::erase_impl()
{
    const auto& id = get_id();
    if (snapshot_exists(vm_name, id))
    {
        const auto qvm_name = QString::fromStdString(vm_name);
        mpu::process_throw_on_error("VBoxManage",
                                    {"snapshot", qvm_name, "delete", QString::fromStdString(id)},
                                    "Could not delete snapshot: {}",
                                    qvm_name);
    }
    else
        mpl::warn(vm_name,
                  "Could not find underlying VirtualBox snapshot for \"{}\". Ignoring...",
                  get_name());
}

void multipass::VirtualBoxSnapshot::apply_impl()
{
    const auto qid = QString::fromStdString(get_id());
    const auto qvm_name = QString::fromStdString(vm_name);
    mpu::process_throw_on_error("VBoxManage",
                                {"snapshot", qvm_name, "restore", qid},
                                "Could not restore snapshot: {}",
                                qvm_name);
}
