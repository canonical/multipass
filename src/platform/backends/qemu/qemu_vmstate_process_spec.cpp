/*
 * Copyright (C) 2020-2021 Canonical, Ltd.
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

mp::QemuVmStateProcessSpec::QemuVmStateProcessSpec(const QString& file_name, const QString& host_arch)
    : QemuBaseProcessSpec{host_arch}, file_name{file_name}
{
}

QStringList mp::QemuVmStateProcessSpec::arguments() const
{
    return {"-nographic", "-dump-vmstate", file_name};
}
