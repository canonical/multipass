/*
 * Copyright (C) 2017 Canonical, Ltd.
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
 * Authored by: Chris Townsend <christopher.townsend@canonical.com>
 *              Alberto Aguirre <alberto.aguirre@canonical.com>
 *
 */

#include <multipass/url_downloader.h>

#include <fmt/format.h>
#include <multipass/logging/log.h>

#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QNetworkReply>
#include <QTimer>
#include <QUrl>

#include <memory>

namespace mp = multipass;
namespace mpl = multipass::logging;

namespace
{
constexpr auto category = "url downloader";

template <typename Action, typename ErrorAction>
QByteArray download(QNetworkAccessManager& manager, QUrl const& url, Action&& action, ErrorAction&& on_error)
{
    QEventLoop event_loop;
    QTimer download_timeout;

    QNetworkRequest request{url};
    request.setRawHeader("Connection", "Keep-Alive");
    request.setAttribute(QNetworkRequest::HttpPipeliningAllowedAttribute, true);
    request.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);

    auto reply = std::unique_ptr<QNetworkReply>(manager.get(request));

    QObject::connect(reply.get(), &QNetworkReply::finished, &event_loop, &QEventLoop::quit);
    QObject::connect(reply.get(), &QNetworkReply::downloadProgress, [&](qint64 bytes_received, qint64 bytes_total) {
        action(reply.get(), download_timeout, bytes_received, bytes_total);
    });
    QObject::connect(&download_timeout, &QTimer::timeout, [&]() {
        download_timeout.stop();
        reply->abort();
    });

    download_timeout.start(10000);
    event_loop.exec();
    if (reply->error() != QNetworkReply::NoError)
    {
        on_error();
        if (!download_timeout.isActive())
            throw std::runtime_error("Network timeout");
        else
            throw std::runtime_error(reply->errorString().toStdString());
    }
    return reply->readAll();
}
}

mp::URLDownloader::URLDownloader(const mp::Path& cache_dir)
{
    network_cache.setCacheDirectory(QDir(cache_dir).filePath("network-cache"));
    manager.setCache(&network_cache);
}

void mp::URLDownloader::download_to(const QUrl& url, const QString& file_name, int64_t size, const int download_type,
                                    const mp::ProgressMonitor& monitor)
{
    QFile file{file_name};
    file.open(QIODevice::ReadWrite | QIODevice::Truncate);

    auto progress_monitor = [&file, &monitor, download_type, size](QNetworkReply* reply, QTimer& download_timeout,
                                                                   qint64 bytes_received, qint64 bytes_total) {
        if (bytes_received == 0)
            return;

        if (download_timeout.isActive())
            download_timeout.stop();
        else
            return;

        if (bytes_total == -1 && size > 0)
            bytes_total = size;
        auto progress = (size < 0) ? size : (100 * bytes_received + bytes_total / 2) / bytes_total;
        if (file.write(reply->readAll()) < 0)
        {
            mpl::log(mpl::Level::error, category,
                     fmt::format("error writing image: {}", file.errorString().toStdString()));
            reply->abort();
        }
        if (!monitor(download_type, progress))
        {
            reply->abort();
        }

        download_timeout.start(10000);
    };

    auto on_error = [&file]() { file.remove(); };

    ::download(manager, url, progress_monitor, on_error);
}

QByteArray mp::URLDownloader::download(const QUrl& url)
{
    return ::download(manager, url, [](QNetworkReply*, QTimer&, qint64, qint64) {}, [] {});
}
