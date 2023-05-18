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

#include "qemu_snapshot.h"
#include <multipass/platform.h>
#include <multipass/process/simple_process_spec.h>

namespace mp = multipass;
namespace mpp = mp::platform;

namespace
{
const auto snapshot_template = QStringLiteral("@%1"); /* avoid colliding with suspension snapshots; prefix with a char
                                                         that can't be part of the name, to avoid confusion */
}

mp::QemuSnapshot::QemuSnapshot(const std::string& name, const std::string& comment,
                               std::shared_ptr<const Snapshot> parent, const VMSpecs& specs, const QString& image_path)
    : BaseSnapshot(name, comment, std::move(parent), specs), image_path{image_path}
{
    /* TODO@snapshots verify no snapshot with the same tag exists yet (otherwise creation would succeed and then we'd be
       unable to identify the snapshot by name) */
    auto process = mpp::make_process(
        mp::simple_process_spec("qemu-img", // TODO@ricab extract making spec
                                QStringList{"snapshot", "-c", snapshot_template.arg(get_name().c_str()), image_path}));

    auto process_state = process->execute();
    if (!process_state.completed_successfully())
    {
        throw std::runtime_error(fmt::format("Internal error: qemu-img failed ({}) with output:\n{}",
                                             process_state.failure_message(), process->read_all_standard_error()));
    }

    // TODO@ricab consider a dedicated method to shoot the snapshot on the backend
}

void multipass::QemuSnapshot::shoot()
{
    // TODO@snapshots lock
}
