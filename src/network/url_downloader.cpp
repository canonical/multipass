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

#include <multipass/url_downloader.h>

#include <multipass/exceptions/aborted_download_exception.h>
#include <multipass/exceptions/download_exception.h>
#include <multipass/file_ops.h>
#include <multipass/format.h>
#include <multipass/logging/log.h>
#include <multipass/platform.h>
#include <multipass/utils.h>
#include <multipass/version.h>

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

[[nodiscard]] QUrl make_http_url_https(const QUrl& url)
{
    QUrl out{url};

    if (out.scheme() == "http")
        out.setScheme("https");

    return out;
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
QByteArray
download(QNetworkAccessManager* manager, const Time& timeout, QUrl const& url, ProgressAction&& on_progress,
         DownloadAction&& on_download, ErrorAction&& on_error, const std::atomic_bool& abort_download,
         const QNetworkRequest::CacheLoadControl cache_load_control = QNetworkRequest::CacheLoadControl::PreferNetwork)
{
    QTimer download_timeout;
    download_timeout.setInterval(timeout);

    const QUrl adjusted_url{make_http_url_https(url)};

    QNetworkRequest request{adjusted_url};
    request.setRawHeader("Connection", "Keep-Alive");
    request.setAttribute(QNetworkRequest::HttpPipeliningAllowedAttribute, true);
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, cache_load_control);
    request.setHeader(
        QNetworkRequest::UserAgentHeader,
        QString::fromStdString(fmt::format("Multipass/{} ({}; {})", multipass::version_string,
                                           mp::platform::host_version(), QSysInfo::currentCpuArchitecture())));

    NetworkReplyUPtr reply{manager->get(request)};

    QObject::connect(reply.get(), &QNetworkReply::downloadProgress, [&](qint64 bytes_received, qint64 bytes_total) {
        on_progress(reply.get(), bytes_received, bytes_total);
    });
    QObject::connect(reply.get(), &QNetworkReply::readyRead, [&]() { on_download(reply.get(), download_timeout); });

    wait_for_reply(reply.get(), download_timeout);

    if (reply->error() != QNetworkReply::NoError)
    {
        const auto error_code = reply->error();
        const auto error_string = reply->errorString().toStdString();

        // Log the original error message at debug level
        mpl::log(mpl::Level::debug,
                 category,
                 fmt::format("Qt error {}: {}", mp::utils::qenum_to_string(error_code), error_string));

        mpl::log(mpl::Level::debug,
                 category,
                 fmt::format("download_timeout is {}active", download_timeout.isActive() ? "" : "in"));

        if (reply->error() == QNetworkReply::ProxyAuthenticationRequiredError || abort_download)
        {
            on_error();
            throw mp::AbortedDownloadException{error_string};
        }
        if (cache_load_control == QNetworkRequest::CacheLoadControl::AlwaysCache)
        {
            on_error();
            // Log at error level since we are giving up
            mpl::log(mpl::Level::error,
                     category,
                     fmt::format("Failed to get {}: {}", adjusted_url.toString(), error_string));
            throw mp::DownloadException{adjusted_url.toString().toStdString(), error_string};
        }
        // Log at warning level when we are going to retry
        mpl::log(mpl::Level::warning,
                 category,
                 fmt::format("Failed to get {}: {} - trying cache.", adjusted_url.toString(), error_string));
        return ::download(manager,
                          timeout,
                          adjusted_url,
                          on_progress,
                          on_download,
                          on_error,
                          abort_download,
                          QNetworkRequest::CacheLoadControl::AlwaysCache);
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

    const QUrl adjusted_url = make_http_url_https(url);
    QNetworkRequest request{adjusted_url};

    NetworkReplyUPtr reply{manager->head(request)};

    wait_for_reply(reply.get(), download_timeout);

    if (reply->error() != QNetworkReply::NoError)
    {
        const auto error_code = reply->error();
        const auto error_string = reply->errorString().toStdString();

        // Log the original error message at debug level
        mpl::log(mpl::Level::debug,
                 category,
                 fmt::format("Qt error {}: {}", mp::utils::qenum_to_string(error_code), error_string));

        mpl::log(mpl::Level::debug,
                 category,
                 fmt::format("download_timeout is {}active", download_timeout.isActive() ? "" : "in"));

        // Log at error level when we give up on getting headers
        mpl::log(mpl::Level::error,
                 category,
                 fmt::format("Cannot retrieve headers for {}: {}", adjusted_url.toString(), error_string));

        throw mp::DownloadException{adjusted_url.toString().toStdString(), error_string};
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
    std::atomic_bool abort_download{false};
    auto manager{MP_NETMGRFACTORY.make_network_manager(cache_dir_path)};

    QFile file{file_name};
    file.open(QIODevice::ReadWrite | QIODevice::Truncate);

    auto progress_monitor = [this, &abort_download, &monitor, download_type,
                             size](QNetworkReply* reply, qint64 bytes_received, qint64 bytes_total) {
        static int last_progress_printed = -1;
        if (bytes_received == 0)
            return;

        if (bytes_total == -1 && size > 0)
            bytes_total = size;

        auto progress = (size < 0) ? size : (100 * bytes_received + bytes_total / 2) / bytes_total;

        abort_download = abort_downloads || (last_progress_printed != progress && !monitor(download_type, progress));
        last_progress_printed = progress;

        if (abort_download)
        {
            reply->abort();
        }
    };

    auto on_download = [this, &abort_download, &file](QNetworkReply* reply, QTimer& download_timeout) {
        abort_download = abort_download || abort_downloads;

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
            abort_download = true;
            reply->abort();
        }
        download_timeout.start();
    };

    auto on_error = [&file]() { file.remove(); };

    ::download(manager.get(), timeout, url, progress_monitor, on_download, on_error, abort_download);
}

QByteArray mp::URLDownloader::download(const QUrl& url)
{
    return download(url, false);
}

QByteArray mp::URLDownloader::download(const QUrl& url, const bool is_force_update_from_network)
{
    auto manager{MP_NETMGRFACTORY.make_network_manager(cache_dir_path)};

    // This will connect to the QNetworkReply::readReady signal and when emitted,
    // reset the timer.
    auto on_download = [this](QNetworkReply* reply, QTimer& download_timeout) {
        if (abort_downloads)
        {
            reply->abort();
            return;
        }

        download_timeout.start();
    };

    const QNetworkRequest::CacheLoadControl cache_load_control = is_force_update_from_network
                                                                     ? QNetworkRequest::CacheLoadControl::AlwaysNetwork
                                                                     : QNetworkRequest::CacheLoadControl::PreferNetwork;

    return ::download(
        manager.get(), timeout, url, [](QNetworkReply*, qint64, qint64) {}, on_download, [] {}, abort_downloads,
        cache_load_control);
}

QDateTime mp::URLDownloader::last_modified(const QUrl& url)
{
    auto manager{MP_NETMGRFACTORY.make_network_manager(cache_dir_path)};

    return get_header(manager.get(), url, QNetworkRequest::LastModifiedHeader, timeout).toDateTime();
}

void mp::URLDownloader::abort_all_downloads()
{
    abort_downloads = true;
}
