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

#pragma once

#include <QObject>
#include <memory>

namespace multipass
{

/*
 * A unique_ptr for Qt objects that are more safely cleaned up on the event loop
 * e.g. QThread
 */

struct QtDeleteLater
{
    void operator()(QObject* o)
    {
        // Detach the signal handlers since this object doesn't live in a QT object
        // hierarchy, and that might cause lifetime related issues, especially
        // if signals are involved.
        o->disconnect();
        // Qt requires that a QObject be deleted from the thread it lives in.
        o->deleteLater();
    }
};

template <typename T>
using qt_delete_later_unique_ptr = std::unique_ptr<T, QtDeleteLater>;

} // namespace multipass
