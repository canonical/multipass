/*
 * Copyright (C) 2017-2019 Canonical, Ltd.
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

#ifndef MULTIPASS_VM_IMAGE_INFO_H
#define MULTIPASS_VM_IMAGE_INFO_H

#include <QString>
#include <QStringList>

namespace multipass
{
class VMImageInfo
{
public:
    QStringList aliases;
    QString os;
    QString release;
    QString release_title;
    bool supported;
    QString image_location;
    QString kernel_location;
    QString initrd_location;
    QString id;
    QString version;
    int64_t size;
    bool verify;
};
}
#endif // MULTIPASS_VM_IMAGE_INFO_H
