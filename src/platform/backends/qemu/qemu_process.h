/*
 * Copyright (C) 2018 Canonical, Ltd.
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

#ifndef QEMU_PROCESS_H
#define QEMU_PROCESS_H

#include <multipass/virtual_machine_description.h>

#include "apparmored_process.h"

class QemuProcess : public AppArmoredProcess
{
    Q_OBJECT
public:
    explicit QemuProcess(const multipass::VirtualMachineDescription& desc, const QString& tap_device_name,
                         const QString& mac_addr);

    QString program() const override;
    QStringList arguments() const override;
    QString apparmor_profile() const override;
    QString identifier() const override;

private:
    const multipass::VirtualMachineDescription desc;
    const QString tap_device_name;
    const QString mac_addr;
};

#endif // QEMU_PROCESS_H
