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

#include "singleton.h"

#include <QStandardPaths>

#define MP_STDPATHS multipass::StandardPaths::instance()

namespace multipass
{
class StandardPaths : public Singleton<StandardPaths>
{
public:
    using LocateOption = QStandardPaths::LocateOption;
    using enum QStandardPaths::LocateOption;

    using LocateOptions = QStandardPaths::LocateOptions;

    using StandardLocation = QStandardPaths::StandardLocation;
    using enum QStandardPaths::StandardLocation;

    StandardPaths(const Singleton<StandardPaths>::PrivatePass&) noexcept;

    virtual QString locate(StandardLocation type,
                           const QString& fileName,
                           LocateOptions options = LocateOption::LocateFile) const;
    virtual QStringList standardLocations(StandardLocation type) const;
    virtual QString writableLocation(StandardLocation type) const;
};
} // namespace multipass
