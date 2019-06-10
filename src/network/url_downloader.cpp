/*
 * Copyright (C) 2017-2019 Canonical, Ltd.
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

#include <multipass/url_downloader.h>

#include <multipass/exceptions/download_exception.h>
#include <multipass/logging/log.h>

#include <multipass/format.h>

#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>
#include <QUrl>

#include <memory>

namespace mp = multipass;
namespace mpl = multipass::logging;

namespace
{
constexpr auto category = "url downloader";

auto make_network_manager()
{
    return std::make_unique<QNetworkAccessManager>();
}

template <typename ProgressAction, typename DownloadAction, typename ErrorAction, typename Time>
QByteArray download(QNetworkAccessManager* manager, const Time& timeout, QUrl const& url, ProgressAction&& on_progress,
                    DownloadAction&& on_download, ErrorAction&& on_error)
{
    QEventLoop event_loop;
    QTimer download_timeout;
    download_timeout.setInterval(timeout);

    QNetworkRequest request{url};
    request.setRawHeader("Connection", "Keep-Alive");
    request.setAttribute(QNetworkRequest::HttpPipeliningAllowedAttribute, true);
    request.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);

    auto reply = manager->get(request);

    QObject::connect(reply, &QNetworkReply::finished, &event_loop, &QEventLoop::quit);
    QObject::connect(reply, &QNetworkReply::downloadProgress, [&](qint64 bytes_received, qint64 bytes_total) {
        on_progress(reply, bytes_received, bytes_total);
    });
    QObject::connect(reply, &QNetworkReply::readyRead, [&]() { on_download(reply, download_timeout); });
    QObject::connect(&download_timeout, &QTimer::timeout, [&]() {
        download_timeout.stop();
        reply->abort();
    });

    download_timeout.start();
    event_loop.exec();
    if (reply->error() != QNetworkReply::NoError)
    {
        on_error();

        const auto msg = download_timeout.isActive() ? reply->errorString().toStdString() : "Network timeout";
        throw mp::DownloadException{url.toString().toStdString(), msg};
    }
    return reply->readAll();
}
} // namespace

mp::URLDownloader::URLDownloader(std::chrono::milliseconds timeout) : URLDownloader{Path(), timeout}
{
}

mp::URLDownloader::URLDownloader(const mp::Path& cache_dir, std::chrono::milliseconds timeout)
    : cache_dir_path{QDir(cache_dir).filePath("network-cache")}, timeout{timeout}
{

    // TODO: We will be using our own caching in the future due to the issue
    //       seen in https://github.com/CanonicalLtd/multipass/issues/825
    if (!cache_dir_path.isEmpty())
    {
        // Clean up old QNetworkDiskCache dirs
        QDir data_dir{QDir(cache_dir_path).filePath("data8")};
        if (data_dir.exists())
        {
            data_dir.removeRecursively();
        }

        QDir prepared_dir{QDir(cache_dir_path).filePath("prepared")};
        if (prepared_dir.exists())
        {
            prepared_dir.removeRecursively();
        }
    }
}

void mp::URLDownloader::download_to(const QUrl& url, const QString& file_name, int64_t size, const int download_type,
                                    const mp::ProgressMonitor& monitor)
{
    auto manager{make_network_manager()};

    QFile file{file_name};
    file.open(QIODevice::ReadWrite | QIODevice::Truncate);

    auto progress_monitor = [&monitor, download_type, size](QNetworkReply* reply, qint64 bytes_received,
                                                            qint64 bytes_total) {
        if (bytes_received == 0)
            return;

        if (bytes_total == -1 && size > 0)
            bytes_total = size;

        auto progress = (size < 0) ? size : (100 * bytes_received + bytes_total / 2) / bytes_total;
        if (!monitor(download_type, progress))
        {
            reply->abort();
        }
    };

    auto on_download = [&file](QNetworkReply* reply, QTimer& download_timeout) {
        if (download_timeout.isActive())
            download_timeout.stop();
        else
            return;

        if (file.write(reply->readAll()) < 0)
        {
            mpl::log(mpl::Level::error, category, fmt::format("error writing image: {}", file.errorString()));
            reply->abort();
        }
        download_timeout.start();
    };

    auto on_error = [&file]() { file.remove(); };

    ::download(manager.get(), timeout, url, progress_monitor, on_download, on_error);
}

QByteArray mp::URLDownloader::download(const QUrl& url)
{
    auto manager{make_network_manager()};

    // This will connect to the QNetworkReply::readReady signal and when emitted,
    // reset the timer.
    auto on_download = [](QNetworkReply*, QTimer& download_timeout) { download_timeout.start(); };

    return ::download(manager.get(), timeout, url, [](QNetworkReply*, qint64, qint64) {}, on_download, [] {});
}

QDateTime mp::URLDownloader::last_modified(const QUrl& url)
{
    auto manager{make_network_manager()};

    QEventLoop event_loop;

    QNetworkRequest request{url};
    request.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);

    auto reply = manager->head(request);
    QObject::connect(reply, &QNetworkReply::finished, &event_loop, &QEventLoop::quit);

    event_loop.exec();

    if (reply->error() != QNetworkReply::NoError)
        throw mp::DownloadException{url.toString().toStdString(), reply->errorString().toStdString()};

    return reply->header(QNetworkRequest::LastModifiedHeader).toDateTime();
}
