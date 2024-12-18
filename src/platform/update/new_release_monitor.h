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

#ifndef MULTIPASS_NEW_RELEASE_MONITOR_H
#define MULTIPASS_NEW_RELEASE_MONITOR_H

#include <multipass/new_release_info.h>
#include <multipass/qt_delete_later_unique_ptr.h>

#include <QObject>
#include <QString>
#include <QTimer>

#include <optional>

namespace multipass
{
class LatestReleaseChecker;

/*
 * NewReleaseMonitor - monitors for new Multipass releases
 *
 * Checks for new releases occur on an internal thread to avoid blocking
 * callers. Rechecks happen at a specified intervals.
 *
 * TODO: the internal thread can be removed when refactoring to move all
 * blocking calls off the main-loop.
 */

class NewReleaseMonitor : public QObject
{
    Q_OBJECT
public:
    static constexpr auto default_update_url = "https://canonical.com/static/files/latest-multipass-releases.json";

    NewReleaseMonitor(const QString& current_version, std::chrono::steady_clock::duration refresh_rate,
                      const QString& update_url = default_update_url);
    ~NewReleaseMonitor();

    std::optional<NewReleaseInfo> get_new_release() const;

private slots:
    void check_for_new_release();
    void latest_release_found(const NewReleaseInfo& latest_release);

private:
    const QString current_version, update_url;
    std::optional<NewReleaseInfo> new_release;
    QTimer refresh_timer;

    qt_delete_later_unique_ptr<LatestReleaseChecker> worker_thread;
};
} // namespace multipass

#endif // MULTIPASS_NEW_RELEASE_MONITOR_H
