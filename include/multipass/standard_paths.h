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

#ifndef MULTIPASS_STANDARD_PATHS_H
#define MULTIPASS_STANDARD_PATHS_H

#include "singleton.h"

#include <QStandardPaths>

#define MP_STDPATHS multipass::StandardPaths::instance()

namespace multipass
{
class StandardPaths : public Singleton<StandardPaths>
{
public:
    // TODO try to replace all the stuff below with `using enum` in C++20 (P1099R5)
    using LocateOption = QStandardPaths::LocateOption;
    static constexpr auto LocateFile = LocateOption::LocateFile;
    static constexpr auto LocateDirectory = LocateOption::LocateDirectory;
    using LocateOptions = QStandardPaths::LocateOptions;

    using StandardLocation = QStandardPaths::StandardLocation;
    static constexpr auto DesktopLocation = StandardLocation::DesktopLocation;
    static constexpr auto DocumentsLocation = StandardLocation::DocumentsLocation;
    static constexpr auto FontsLocation = StandardLocation::FontsLocation;
    static constexpr auto ApplicationsLocation = StandardLocation::ApplicationsLocation;
    static constexpr auto MusicLocation = StandardLocation::MusicLocation;
    static constexpr auto MoviesLocation = StandardLocation::MoviesLocation;
    static constexpr auto PicturesLocation = StandardLocation::PicturesLocation;
    static constexpr auto TempLocation = StandardLocation::TempLocation;
    static constexpr auto HomeLocation = StandardLocation::HomeLocation;
    static constexpr auto CacheLocation = StandardLocation::CacheLocation;
    static constexpr auto GenericCacheLocation = StandardLocation::GenericCacheLocation;
    static constexpr auto GenericDataLocation = StandardLocation::GenericDataLocation;
    static constexpr auto RuntimeLocation = StandardLocation::RuntimeLocation;
    static constexpr auto ConfigLocation = StandardLocation::ConfigLocation;
    static constexpr auto DownloadLocation = StandardLocation::DownloadLocation;
    static constexpr auto GenericConfigLocation = StandardLocation::GenericConfigLocation;
    static constexpr auto AppDataLocation = StandardLocation::AppDataLocation;
    static constexpr auto AppLocalDataLocation = StandardLocation::AppLocalDataLocation;
    static constexpr auto AppConfigLocation = StandardLocation::AppConfigLocation;

    StandardPaths(const Singleton<StandardPaths>::PrivatePass&) noexcept;

    virtual QString locate(StandardLocation type, const QString& fileName,
                           LocateOptions options = LocateOption::LocateFile) const;
    virtual QStringList standardLocations(StandardLocation type) const;
    virtual QString writableLocation(StandardLocation type) const;
};
} // namespace multipass
#endif // MULTIPASS_STANDARD_PATHS_H
