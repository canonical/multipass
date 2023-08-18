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

void require_unique_id(mp::PowerShell* ps, const QString& vm_name, const QString& id)
{
    static const QString expected_error{"ObjectNotFound"};

    QString output;
    if (ps->run({"Get-VMCheckpoint", "-VMName", vm_name, "-Name", id}, &output))
        throw std::runtime_error{fmt::format("A snapshot called {} already exists for this VM in Hyper-V", id)};

    if (!output.contains(expected_error))
    {
        mpl::log(mpl::Level::warning, vm_name.toStdString(),
                 fmt::format("Get-VMCheckpoint failed with unexpected output: {}", output));
        throw std::runtime_error{"Could not verify snapshot-name uniqueness"};
    }

    return; // we're good: the command failed with the expected error
}
} // namespace

mp::HyperVSnapshot::HyperVSnapshot(const std::string& name, const std::string& comment, const VMSpecs& specs,
                                   std::shared_ptr<Snapshot> parent, const QString& vm_name, PowerShell* power_shell)
    : BaseSnapshot{name, comment, specs, std::move(parent)}, vm_name{vm_name}, power_shell{power_shell}
{
    assert(power_shell);
}

void mp::HyperVSnapshot::capture_impl()
{
    auto id = quoted(derive_id());
    require_unique_id(power_shell, vm_name, id);
    power_shell->easy_run({"Checkpoint-VM", "-Name", vm_name, "-SnapshotName", id}, "Could not create snapshot");
}

void mp::HyperVSnapshot::erase_impl()
{
    throw NotImplementedOnThisBackendException{"erase"}; // TODO@snapshots
}

void mp::HyperVSnapshot::apply_impl()
{
    throw NotImplementedOnThisBackendException{"apply"}; // TODO@snapshots
}
