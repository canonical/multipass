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

#include "new_release_monitor.h"

#include <multipass/exceptions/download_exception.h>
#include <multipass/logging/log.h>
#include <multipass/url_downloader.h>

#include <semver200.h>

#include <QJsonDocument>
#include <QJsonObject>
#include <QThread>

namespace mp = multipass;
namespace mpl = multipass::logging;

namespace
{
constexpr auto timeout = std::chrono::minutes(1);
constexpr auto json_tag_name = "version";
constexpr auto json_html_url = "release_url";
constexpr auto json_title = "title";
constexpr auto json_description = "description";

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
    LatestReleaseChecker(QString update_url) : update_url(update_url)
    {
    }

    void run() override
    {
        QUrl url(update_url);
        try
        {
            mp::URLDownloader downloader(::timeout);
            QByteArray json = downloader.download(url);
            const auto manifest = ::parse_manifest(json);
            if (!manifest.contains(::json_tag_name) || !manifest.contains(::json_html_url))
                throw std::runtime_error("Update JSON missing required fields");

            mp::NewReleaseInfo release;
            release.version = manifest[::json_tag_name].toString();
            release.url = manifest[::json_html_url].toString();
            release.title = manifest[::json_title].toString();
            release.description = manifest[::json_description].toString();

            mpl::log(mpl::Level::debug, "update",
                     fmt::format("Latest Multipass release available is version {}", qUtf8Printable(release.version)));

            emit latest_release_found(release);
        }
        catch (const mp::DownloadException& e)
        {
            mpl::log(mpl::Level::info, "update",
                     fmt::format("Failed to fetch update info: {}", qUtf8Printable(e.what())));
        }
        catch (const std::runtime_error& e)
        {
            mpl::log(mpl::Level::info, "update",
                     fmt::format("Failed to parse update info: {}", qUtf8Printable(e.what())));
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

std::optional<mp::NewReleaseInfo> mp::NewReleaseMonitor::get_new_release() const
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
                     fmt::format("A New Multipass release is available: {}", qUtf8Printable(new_release->version)));
        }
    }
    catch (const version::Parse_error& e)
    {
        mpl::log(mpl::Level::warning, "update",
                 fmt::format("Version strings {} and {} not comparable: {}", qUtf8Printable(current_version),
                             qUtf8Printable(latest_release.version), e.what()));
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
