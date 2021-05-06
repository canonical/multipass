/*
 * Copyright (C) 2017-2021 Canonical, Ltd.
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

#include "daemon_settings_monitor.h"

#include <multipass/settings.h>
#include <multipass/utils.h>

#include <QCoreApplication>
#include <QObject>

namespace mp = multipass;

namespace
{
constexpr const int settings_changed_code = 42;
} // namespace

namespace multipass
{
DaemonSettingsMonitor::DaemonSettingsMonitor(const std::string& current_driver) // temporary
{
    const auto filename = MP_SETTINGS.get_daemon_settings_file_path();
    mp::utils::check_and_create_config_file(filename); // create if not there

    watcher.addPath(filename);

    QObject::connect(&watcher, &QFileSystemWatcher::fileChanged, [this, current_driver, filename] {
        if (mp::utils::get_driver_str() != current_driver.c_str())
            MP_UTILS.exit(settings_changed_code);

        if (!watcher.files().contains(filename))
            watcher.addPath(filename);
    });
}
} // namespace multipass
