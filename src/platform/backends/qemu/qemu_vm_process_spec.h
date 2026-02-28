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

#include "qemu_base_process_spec.h"
#include "qemu_virtual_machine.h"

#include <multipass/virtual_machine_description.h>

#include <optional>

namespace multipass
{

class QemuVMProcessSpec : public QemuBaseProcessSpec
{
public:
    struct ResumeData
    {
        QString suspend_tag;
        QString machine_type;
        QStringList arguments;
    };

    static QString default_machine_type();

    explicit QemuVMProcessSpec(const VirtualMachineDescription& desc,
                               const QStringList& platform_args,
                               const QemuVirtualMachine::MountArgs& mount_args,
                               const std::optional<ResumeData>& resume_data);

    QStringList arguments() const override;

    QString apparmor_profile() const override;
    QString identifier() const override;
    QString working_directory() const override;

private:
    const VirtualMachineDescription desc;
    const QStringList platform_args;
    const QemuVirtualMachine::MountArgs mount_args;
    const std::optional<ResumeData> resume_data;
};

} // namespace multipass
