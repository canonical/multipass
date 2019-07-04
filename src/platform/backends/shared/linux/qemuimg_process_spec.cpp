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

#include "qemuimg_process_spec.h"

namespace mp = multipass;

mp::QemuImgProcessSpec::QemuImgProcessSpec(const QStringList& args) : args{args}
{
}

QString mp::QemuImgProcessSpec::program() const
{
    return "qemu-img";
}

QStringList mp::QemuImgProcessSpec::arguments() const
{
    return args;
}
