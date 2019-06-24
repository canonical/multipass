/*
 * Copyright (C) 2019 Canonical, Ltd.
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

#ifndef MULTIPASS_QEMU_PROCESS_H
#define MULTIPASS_QEMU_PROCESS_H

#include <shared/linux/process_spec.h>

#include <multipass/optional.h>
#include <multipass/virtual_machine_description.h>

namespace multipass
{

class QemuVMProcessSpec : public ProcessSpec
{
public:
    struct ResumeData
    {
        QString suspend_tag;
        QString machine_type;
    };

    static int latest_version();
    static QString default_machine_type();

    explicit QemuVMProcessSpec(const VirtualMachineDescription& desc, int version, const QString& tap_device_name,
                               const multipass::optional<ResumeData>& resume_data);

    QString program() const override;
    QStringList arguments() const override;
    QString working_directory() const override;

private:
    const VirtualMachineDescription desc;
    const int version;
    const QString tap_device_name;
    const multipass::optional<ResumeData> resume_data;
};

} // namespace multipass

#endif // MULTIPASS_QEMU_PROCESS_H
