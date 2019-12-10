/*
 * Copyright (C) 2019 Canonical, Ltd.
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

#include "new_release_monitor.h"

#include <multipass/exceptions/download_exception.h>
#include <multipass/logging/log.h>
#include <multipass/url_downloader.h>
#include <multipass/format.h>

#include <yaml-cpp/yaml.h>

#include <semver200.h>

#include <QThread>

namespace mp = multipass;
namespace mpl = multipass::logging;

namespace
{
constexpr auto timeout = std::chrono::minutes(1);
constexpr auto yaml_tag_name = "version";
constexpr auto yaml_html_url = "release_url";

} // namespace

class mp::LatestReleaseChecker : public QThread
{
    Q_OBJECT
public:
    LatestReleaseChecker(QString update_url) : update_url(update_url)
    {
    }

    void run() override
    {
        QUrl url(update_url);
        try
        {
            mp::URLDownloader downloader(::timeout);
            const auto manifest = YAML::Load(downloader.download(url).toStdString());
            if (!manifest[yaml_tag_name] || !manifest[yaml_html_url])
                throw std::runtime_error("Update YAML missing required fields");

            mp::NewReleaseInfo release;
            release.version = QString::fromStdString(manifest[yaml_tag_name].as<std::string>());
            if (release.version[0] == 'v')
                release.version.remove(0, 1);

            release.url = QString::fromStdString(manifest[yaml_html_url].as<std::string>());

            mpl::log(mpl::Level::debug, "update",
                     fmt::format("Latest Multipass release available is version {}", qPrintable(release.version)));

            emit latest_release_found(release);
        }
        catch (const mp::DownloadException& e)
        {
            mpl::log(mpl::Level::warning, "update", fmt::format("Failed to fetch update info: {}", e.what()));
        }
        catch (const std::runtime_error& e) // This covers YAML::Exception
        {
            mpl::log(mpl::Level::warning, "update", fmt::format("Failed to parse update info: {}", e.what()));
        }
    }
signals:
    void latest_release_found(const multipass::NewReleaseInfo& release);

private:
    const QString update_url;
};

mp::NewReleaseMonitor::NewReleaseMonitor(const QString& current_version,
                                         std::chrono::steady_clock::duration refresh_rate, const QString& update_url)
    : current_version(current_version), update_url(update_url)
{
    qRegisterMetaType<multipass::NewReleaseInfo>(); // necessary to allow custom type be passed in signal/slot

    check_for_new_release();

    refresh_timer.setInterval(std::chrono::duration_cast<std::chrono::milliseconds>(refresh_rate));
    connect(&refresh_timer, &QTimer::timeout, this, &mp::NewReleaseMonitor::check_for_new_release);
    refresh_timer.start();
}

mp::NewReleaseMonitor::~NewReleaseMonitor() = default;

mp::optional<mp::NewReleaseInfo> mp::NewReleaseMonitor::get_new_release() const
{
    return new_release;
}

void mp::NewReleaseMonitor::latest_release_found(const NewReleaseInfo& latest_release)
{
    try
    {
        // Deliberately keeping all version string parsing here. If any version string
        // not of correct form, throw.
        if (version::Semver200_version(current_version.toStdString()) <
            version::Semver200_version(latest_release.version.toStdString()))
        {
            new_release = latest_release;
            mpl::log(mpl::Level::info, "update",
                     fmt::format("A New Multipass release is available: {}", qPrintable(new_release->version)));
        }
    }
    catch (const version::Parse_error& e)
    {
        mpl::log(mpl::Level::warning, "update",
                 fmt::format("Version strings {} and {} not comparable: {}", qPrintable(current_version),
                             qPrintable(latest_release.version), e.what()));
    }
}

void mp::NewReleaseMonitor::check_for_new_release()
{
    if (!worker_thread)
    {
        worker_thread.reset(new mp::LatestReleaseChecker(update_url));
        connect(worker_thread.get(), &mp::LatestReleaseChecker::latest_release_found, this,
                &mp::NewReleaseMonitor::latest_release_found);
        connect(worker_thread.get(), &QThread::finished, this, [this]() { worker_thread.reset(); });
        worker_thread->start();
    }
}

#include "new_release_monitor.moc"
