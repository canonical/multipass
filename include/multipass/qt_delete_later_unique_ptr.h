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

#ifndef QT_DELETE_LATER_UNIQUE_PTR_H
#define QT_DELETE_LATER_UNIQUE_PTR_H

#include <memory>
#include <QObject>

namespace multipass
{

/*
 * A unique_ptr for Qt objects that are more safely cleaned up on the event loop
 * e.g. QThread
 */

struct QtDeleteLater {
    void operator()(QObject *o) {
        o->deleteLater();
    }
};

template<typename T>
using qt_delete_later_unique_ptr = std::unique_ptr<T, QtDeleteLater>;

} // namespace

#endif // QT_DELETE_LATER_UNIQUE_PTR_H
