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

#ifndef MULTIPASS_NEW_RELEASE_INFO_H
#define MULTIPASS_NEW_RELEASE_INFO_H

#include <QMetaType>
#include <QUrl>

namespace multipass
{

struct NewReleaseInfo
{
    QString version;
    QUrl url;
    QString title;
    QString description;
};

} // namespace multipass

Q_DECLARE_METATYPE(multipass::NewReleaseInfo)

#endif // MULTIPASS_NEW_RELEASE_INFO_H
