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
    QString release_codename;
    bool supported;
    QString image_location;
    QString id;
    QString stream_location;
    QString version;
    int64_t size;
    bool verify;
};

inline bool operator==(const VMImageInfo& a, const VMImageInfo& b)
{
    return std::tie(a.aliases,
                    a.os,
                    a.release,
                    a.release_title,
                    a.release_codename,
                    a.supported,
                    a.image_location,
                    a.id,
                    a.stream_location,
                    a.version,
                    a.size,
                    a.verify) == std::tie(b.aliases,
                                          b.os,
                                          b.release,
                                          b.release_title,
                                          b.release_codename,
                                          b.supported,
                                          b.image_location,
                                          b.id,
                                          b.stream_location,
                                          b.version,
                                          b.size,
                                          b.verify);
}
} // namespace multipass
#endif // MULTIPASS_VM_IMAGE_INFO_H
