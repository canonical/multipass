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

#include "qemu_vmstate_process_spec.h"

namespace mp = multipass;

mp::QemuVmStateProcessSpec::QemuVmStateProcessSpec(const QString& file_name,
                                                   const QStringList& platform_args)
    : file_name{file_name}, platform_args{platform_args}
{
}

QStringList mp::QemuVmStateProcessSpec::arguments() const
{
    QStringList args;

    args << platform_args
#if defined Q_PROCESSOR_ARM
         << "-machine"
         << "virt"
#elif defined Q_PROCESSOR_S390
         << "-machine"
         << "s390-ccw-virtio"
#endif
         << "-nographic"
         << "-dump-vmstate" << file_name;

    return args;
}
