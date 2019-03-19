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

#include "version.h"
#include <multipass/exceptions/download_exception.h>
#include <multipass/logging/log.h>
#include <multipass/url_downloader.h>

#include <fmt/format.h>

#include <QJsonDocument>
#include <QJsonObject>
#include <QThread>

namespace mp = multipass;
namespace mpl = multipass::logging;

namespace
{
constexpr auto update_url = "https://api.github.com/repos/CanonicalLtd/multipass/releases/latest";
constexpr auto timeout = std::chrono::minutes(1);

QJsonObject parse_manifest(const QByteArray& json)
{
    QJsonParseError parse_error;
    const auto doc = QJsonDocument::fromJson(json, &parse_error);
    if (doc.isNull())
        throw std::runtime_error(parse_error.errorString().toStdString());

    if (!doc.isObject())
        throw std::runtime_error("invalid JSON object");
    return doc.object();
}
} // namespace

class mp::LatestReleaseChecker : public QThread
{
    Q_OBJECT
public:
    void run()
    {
        QUrl url(update_url);
        try
        {
            mp::URLDownloader downloader(timeout);
            QByteArray json = downloader.download(url);

            const auto manifest = ::parse_manifest(json);
            mp::NewReleaseInfo release;
            release.version = manifest["tag_name"].toString();
            release.url = manifest["html_url"].toString();

            mpl::log(mpl::Level::debug, "update",
                     fmt::format("Latest Multipass release available is version {}", qPrintable(release.version)));

            emit latest_release_found(release);
        }
        catch (const mp::DownloadException& e)
        {
            mpl::log(mpl::Level::info, "update", fmt::format("Failed to check for updates: {}", qPrintable(e.what())));
        }
    }
signals:
    void latest_release_found(const multipass::NewReleaseInfo& release);
};

mp::NewReleaseMonitor::NewReleaseMonitor(const QString& current_version, std::chrono::hours refresh_rate)
    : current_version(current_version)
{
    qRegisterMetaType<multipass::NewReleaseInfo>(); // necessary to allow custom type be passed in signal/slot

    check_for_new_release();

    refresh_timer.setInterval(refresh_rate);
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
    if (current_version < mp::Version(latest_release.version))
    {
        new_release = latest_release;
        mpl::log(mpl::Level::info, "update",
                 fmt::format("A New Multipass release is availble: {}", qPrintable(new_release->version)));
    }
}

void mp::NewReleaseMonitor::check_for_new_release()
{
    if (!worker_thread)
    {
        worker_thread.reset(new mp::LatestReleaseChecker);
        connect(worker_thread.get(), &mp::LatestReleaseChecker::latest_release_found, this,
                &mp::NewReleaseMonitor::latest_release_found);
        connect(worker_thread.get(), &QThread::finished, [this]() { worker_thread.reset(); });
        worker_thread->start();
    }
}

#include "new_release_monitor.moc"
