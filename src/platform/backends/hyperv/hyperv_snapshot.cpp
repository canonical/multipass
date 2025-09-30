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

#include "hyperv_snapshot.h"
#include "hyperv_virtual_machine.h"

#include <shared/windows/powershell.h>

#include <multipass/exceptions/not_implemented_on_this_backend_exception.h> // TODO@snapshots drop
#include <multipass/format.h>
#include <multipass/logging/log.h>

#include <cassert>

namespace mp = multipass;
namespace mpl = mp::logging;

namespace
{
QString quoted(const QString& s)
{
    return '"' + s + '"';
}

bool snapshot_exists(mp::PowerShell& ps, const QString& vm_name, const QString& id)
{
    static const auto expected_error = QStringLiteral("ObjectNotFound");

    QString output_err;
    if (ps.run({"Get-VMCheckpoint", "-VMName", vm_name, "-Name", id}, nullptr, &output_err))
        return true;

    if (!output_err.contains(expected_error))
    {
        mpl::warn(vm_name.toStdString(),
                  fmt::format("Get-VMCheckpoint failed with unexpected output: {}", output_err));
        throw std::runtime_error{"Failure while looking for snapshot name"};
    }

    return false; // we're good: the command failed with the expected error
}

void require_unique_id(mp::PowerShell& ps, const QString& vm_name, const QString& id)
{
    if (snapshot_exists(ps, vm_name, id))
        throw std::runtime_error{
            fmt::format("A snapshot with ID {} already exists for {} in Hyper-V", id, vm_name)};
}
} // namespace

mp::HyperVSnapshot::HyperVSnapshot(const std::string& name,
                                   const std::string& comment,
                                   const std::string& cloud_init_instance_id,
                                   const VMSpecs& specs,
                                   std::shared_ptr<Snapshot> parent,
                                   const QString& vm_name,
                                   HyperVVirtualMachine& vm,
                                   PowerShell& power_shell)
    : BaseSnapshot{name, comment, cloud_init_instance_id, std::move(parent), specs, vm},
      quoted_id{quoted(get_id())},
      vm_name{vm_name},
      power_shell{power_shell}
{
}

mp::HyperVSnapshot::HyperVSnapshot(const QString& filename,
                                   HyperVVirtualMachine& vm,
                                   const VirtualMachineDescription& desc,
                                   PowerShell& power_shell)
    : BaseSnapshot{filename, vm, desc},
      quoted_id{quoted(get_id())},
      vm_name{QString::fromStdString(desc.vm_name)},
      power_shell{power_shell}
{
}

void mp::HyperVSnapshot::capture_impl()
{
    require_unique_id(power_shell, vm_name, quoted_id);
    power_shell.easy_run({"Checkpoint-VM", "-Name", vm_name, "-SnapshotName", quoted_id},
                         "Could not create snapshot");
}

void mp::HyperVSnapshot::erase_impl()
{
    if (snapshot_exists(power_shell, vm_name, quoted_id))
        power_shell.easy_run(
            {"Remove-VMCheckpoint", "-VMName", vm_name, "-Name", quoted_id, "-Confirm:$false"},
            "Could not delete snapshot");
    else
        mpl::warn(vm_name.toStdString(),
                  fmt::format("Could not find underlying Hyper-V snapshot for \"{}\". Ignoring...",
                              get_name()));
}

void mp::HyperVSnapshot::apply_impl()
{
    power_shell.easy_run(
        {"Restore-VMCheckpoint", "-VMName", vm_name, "-Name", quoted_id, "-Confirm:$false"},
        "Could not apply snapshot");
}
