/*
 * Copyright (C) 2020 Canonical, Ltd.
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

#ifndef MULTIPASS_QEMU_DUMP_VMSTATE_PROCESS_SPEC_H
#define MULTIPASS_QEMU_DUMP_VMSTATE_PROCESS_SPEC_H

#include <multipass/process_spec.h>

namespace multipass
{

class QemuDumpVmStateProcessSpec : public ProcessSpec
{
public:
    explicit QemuDumpVmStateProcessSpec(const QString& file_name);

    QString program() const override;
    QStringList arguments() const override;
    QString working_directory() const override;

    QString apparmor_profile() const override
    {
        return QString();
    }

private:
    QString file_name;
};

} // namespace multipass

#endif // MULTIPASS_QEMU_DUMP_VMSTATE_PROCESS_SPEC_H
