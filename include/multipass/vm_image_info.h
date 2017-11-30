/*
 * Copyright (C) 2017 Canonical, Ltd.
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
 * Authored by: Alberto Aguirre <alberto.aguirre@canonical.com>
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
    const QStringList aliases;
    const QString release;
    const QString release_title;
    const bool supported;
    const QString image_location;
    const QString kernel_location;
    const QString initrd_location;
    const QString id;
    const QString version;
    const int64_t size;
};
}
#endif // MULTIPASS_VM_IMAGE_INFO_H
