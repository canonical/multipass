/*
 * Copyright (C) 2020 Canonical, Ltd.
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

#ifndef MULTIPASS_STANDARD_PATHS_H
#define MULTIPASS_STANDARD_PATHS_H

#include "singleton.h"

#include <QStandardPaths>

namespace multipass
{
class StandardPaths : public Singleton<StandardPaths>
{
public:
    using LocateOption = QStandardPaths::LocateOption;
    using LocateOptions = QStandardPaths::LocateOptions;
    using StandardLocation = QStandardPaths::StandardLocation;

    StandardPaths(const Singleton<StandardPaths>::PrivatePass&);

    virtual QString locate(StandardLocation type, const QString& fileName,
                           LocateOptions options = LocateOption::LocateFile) const;
    virtual QStringList standardLocations(StandardLocation type) const;
    virtual QString writableLocation(StandardLocation type) const;
};
} // namespace multipass
#endif // MULTIPASS_STANDARD_PATHS_H
