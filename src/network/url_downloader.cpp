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

#include <multipass/url_downloader.h>

#include <multipass/exceptions/aborted_download_exception.h>
#include <multipass/exceptions/download_exception.h>
#include <multipass/file_ops.h>
#include <multipass/format.h>
#include <multipass/logging/log.h>

#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QNetworkDiskCache>
#include <QNetworkReply>
#include <QTimer>
#include <QUrl>

#include <memory>

namespace mp = multipass;
namespace mpl = multipass::logging;

namespace
{
constexpr auto category = "url downloader";
using NetworkReplyUPtr = std::unique_ptr<QNetworkReply>;

auto make_network_manager(const mp::Path& cache_dir_path)
{
    auto manager = std::make_unique<QNetworkAccessManager>();

    if (!cache_dir_path.isEmpty())
    {
        auto network_cache = new QNetworkDiskCache;
        network_cache->setCacheDirectory(cache_dir_path);

        // Manager now owns network_cache and so it will delete it in its dtor
        manager->setCache(network_cache);
    }

    return manager;
}

void wait_for_reply(QNetworkReply* reply, QTimer& download_timeout)
{
    QEventLoop event_loop;

    QObject::connect(reply, &QNetworkReply::finished, &event_loop, &QEventLoop::quit);
    QObject::connect(&download_timeout, &QTimer::timeout, [reply, &download_timeout]() {
        download_timeout.stop();
        reply->abort();
    });

    download_timeout.start();
    event_loop.exec();
}

template <typename ProgressAction, typename DownloadAction, typename ErrorAction, typename Time>
QByteArray download(QNetworkAccessManager* manager, const Time& timeout, QUrl const& url, ProgressAction&& on_progress,
                    DownloadAction&& on_download, ErrorAction&& on_error, const std::atomic_bool& abort_download,
                    const bool force_cache = false)
{
    QTimer download_timeout;
    download_timeout.setInterval(timeout);

    QNetworkRequest request{url};
    request.setRawHeader("Connection", "Keep-Alive");
    request.setAttribute(QNetworkRequest::HttpPipeliningAllowedAttribute, true);
    request.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute,
                         force_cache ? QNetworkRequest::AlwaysCache : QNetworkRequest::PreferNetwork);

    NetworkReplyUPtr reply{manager->get(request)};

    QObject::connect(reply.get(), &QNetworkReply::downloadProgress, [&](qint64 bytes_received, qint64 bytes_total) {
        on_progress(reply.get(), bytes_received, bytes_total);
    });
    QObject::connect(reply.get(), &QNetworkReply::readyRead, [&]() { on_download(reply.get(), download_timeout); });

    wait_for_reply(reply.get(), download_timeout);

    if (reply->error() != QNetworkReply::NoError)
    {
        const auto msg = download_timeout.isActive() ? reply->errorString().toStdString() : "Network timeout";

        if (reply->error() == QNetworkReply::ProxyAuthenticationRequiredError || abort_download)
        {
            on_error();
            throw mp::AbortedDownloadException{msg};
        }
        else if (force_cache)
        {
            on_error();
            throw mp::DownloadException{url.toString().toStdString(), msg};
        }
        else
        {
            mpl::log(mpl::Level::warning, category,
                     fmt::format("Error getting {}: {} - trying cache.", url.toString(), msg));
            return ::download(manager, timeout, url, on_progress, on_download, on_error, abort_download, true);
        }
    }

    mpl::log(mpl::Level::trace, category,
             fmt::format("Found {} in cache: {}", url.toString(),
                         reply->attribute(QNetworkRequest::SourceIsFromCacheAttribute).toBool()));

    return reply->readAll();
}

template <typename Time>
auto get_header(QNetworkAccessManager* manager, const QUrl& url, const QNetworkRequest::KnownHeaders header,
                const Time& timeout)
{
    QTimer download_timeout;
    download_timeout.setInterval(timeout);

    QNetworkRequest request{url};
    request.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);

    NetworkReplyUPtr reply{manager->head(request)};

    wait_for_reply(reply.get(), download_timeout);

    if (reply->error() != QNetworkReply::NoError)
    {
        const auto msg = download_timeout.isActive() ? reply->errorString().toStdString() : "Network timeout";

        mpl::log(mpl::Level::warning, category, fmt::format("Cannot retrieve headers for {}: {}", url.toString(), msg));

        throw mp::DownloadException{url.toString().toStdString(), reply->errorString().toStdString()};
    }

    return reply->header(header);
}
} // namespace

mp::NetworkManagerFactory::NetworkManagerFactory(const Singleton<NetworkManagerFactory>::PrivatePass& pass) noexcept
    : Singleton<NetworkManagerFactory>::Singleton{pass}
{
}

std::unique_ptr<QNetworkAccessManager>
mp::NetworkManagerFactory::make_network_manager(const mp::Path& cache_dir_path) const
{
    return ::make_network_manager(cache_dir_path);
}

mp::URLDownloader::URLDownloader(std::chrono::milliseconds timeout) : URLDownloader{Path(), timeout}
{
}

mp::URLDownloader::URLDownloader(const mp::Path& cache_dir, std::chrono::milliseconds timeout)
    : cache_dir_path{QDir(cache_dir).filePath("network-cache")}, timeout{timeout}
{
}

void mp::URLDownloader::download_to(const QUrl& url, const QString& file_name, int64_t size, const int download_type,
                                    const mp::ProgressMonitor& monitor)
{
    auto manager{MP_NETMGRFACTORY.make_network_manager(cache_dir_path)};

    QFile file{file_name};
    file.open(QIODevice::ReadWrite | QIODevice::Truncate);

    auto progress_monitor = [this, &monitor, download_type, size](QNetworkReply* reply, qint64 bytes_received,
                                                                  qint64 bytes_total) {
        if (bytes_received == 0)
            return;

        if (bytes_total == -1 && size > 0)
            bytes_total = size;

        auto progress = (size < 0) ? size : (100 * bytes_received + bytes_total / 2) / bytes_total;
        if (!monitor(download_type, progress))
        {
            abort_all_downloads();
            reply->abort();
        }
    };

    auto on_download = [this, &file](QNetworkReply* reply, QTimer& download_timeout) {
        if (abort_download)
        {
            reply->abort();
            return;
        }

        if (download_timeout.isActive())
            download_timeout.stop();
        else
            return;

        if (MP_FILEOPS.write(file, reply->readAll()) < 0)
        {
            mpl::log(mpl::Level::error, category, fmt::format("error writing image: {}", file.errorString()));
            abort_all_downloads();
            reply->abort();
        }
        download_timeout.start();
    };

    auto on_error = [&file]() { file.remove(); };

    ::download(manager.get(), timeout, url, progress_monitor, on_download, on_error, abort_download);
}

QByteArray mp::URLDownloader::download(const QUrl& url)
{
    auto manager{MP_NETMGRFACTORY.make_network_manager(cache_dir_path)};

    // This will connect to the QNetworkReply::readReady signal and when emitted,
    // reset the timer.
    auto on_download = [this](QNetworkReply* reply, QTimer& download_timeout) {
        if (abort_download)
        {
            reply->abort();
            return;
        }

        download_timeout.start();
    };

    return ::download(
        manager.get(), timeout, url, [](QNetworkReply*, qint64, qint64) {}, on_download, [] {}, abort_download);
}

QDateTime mp::URLDownloader::last_modified(const QUrl& url)
{
    auto manager{MP_NETMGRFACTORY.make_network_manager(cache_dir_path)};

    return get_header(manager.get(), url, QNetworkRequest::LastModifiedHeader, timeout).toDateTime();
}

void mp::URLDownloader::abort_all_downloads()
{
    abort_download = true;
}
