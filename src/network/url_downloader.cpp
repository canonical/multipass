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
#include <multipass/version.h>

#include <QDir>
#include <QFile>
#include <QUrl>
#include <QSysInfo>

#include <memory>
#include <Poco/Net/HTTPClientSession.h>
#include <Poco/Net/HTTPSClientSession.h>
#include <Poco/Net/HTTPRequest.h>
#include <Poco/Net/HTTPResponse.h>
#include <Poco/URI.h>
#include <Poco/Exception.h>

namespace mp = multipass;
namespace mpl = multipass::logging;

namespace
{
constexpr auto category = "url downloader";
} // namespace

mp::URLDownloader::URLDownloader(std::chrono::milliseconds timeout) : URLDownloader{Path(), timeout}
{
}

mp::URLDownloader::URLDownloader(const mp::Path& cache_dir, std::chrono::milliseconds timeout)
    : cache_dir_path{QDir(cache_dir).filePath("network-cache")}, timeout{timeout}
{
}

void mp::URLDownloader::download_to(const QUrl& url, const QString& file_name, int64_t size, int download_type,
                                    const mp::ProgressMonitor& monitor)
{
    std::string url_str = url.toString().toStdString();

    try
    {
        Poco::URI uri(url_str);
        int maxRedirects = 10;
        int redirectCount = 0;

        while (redirectCount < maxRedirects)
        {
            std::string path = uri.getPathEtc().empty() ? "/" : uri.getPathEtc();

            // Determine if the scheme is HTTP or HTTPS
            std::string scheme = uri.getScheme();
            std::unique_ptr<Poco::Net::HTTPClientSession> session;

            if (scheme == "https")
            {
                Poco::Net::Context::Ptr context = new Poco::Net::Context(
                    Poco::Net::Context::CLIENT_USE, "", "", "", Poco::Net::Context::VERIFY_NONE);
                session = std::make_unique<Poco::Net::HTTPSClientSession>(uri.getHost(), uri.getPort(), context);
            }
            else
            {
                session = std::make_unique<Poco::Net::HTTPClientSession>(uri.getHost(), uri.getPort());
            }

            // Create and configure request
            Poco::Net::HTTPRequest request(Poco::Net::HTTPRequest::HTTP_GET, path);
            request.set("Connection", "Keep-Alive");
            request.set("User-Agent",
                        QString::fromStdString(fmt::format("Multipass/{} ({}; {})", multipass::version_string,
                                                           mp::platform::host_version(),
                                                           QSysInfo::currentCpuArchitecture()))
                            .toStdString());

            // Send request and receive response
            session->setTimeout(Poco::Timespan(timeout.count() / 1000, (timeout.count() % 1000) * 1000));
            session->sendRequest(request);
            Poco::Net::HTTPResponse response;
            std::istream& rs = session->receiveResponse(response);

            // Check for HTTP OK status
            if (response.getStatus() == Poco::Net::HTTPResponse::HTTP_OK)
            {
                // Open file for writing
                QFile file(file_name);
                if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
                {
                    mpl::log(mpl::Level::error, category,
                             fmt::format("Error opening file {}: {}", file_name.toStdString(),
                                         file.errorString().toStdString()));
                    throw mp::DownloadException{url_str, "Failed to open file for writing"};
                }

                // Read data and write to file with progress monitoring
                char buffer[8192];
                std::streamsize total_bytes_received = 0;
                std::streamsize content_length = response.getContentLength64();
                if (content_length == -1 && size > 0)
                    content_length = size;

                while (!rs.eof() && !abort_downloads)
                {
                    rs.read(buffer, sizeof(buffer));
                    std::streamsize bytes_read = rs.gcount();
                    if (bytes_read > 0)
                    {
                        file.write(buffer, bytes_read);
                        total_bytes_received += bytes_read;

                        // Update progress
                        int progress = (content_length > 0)
                                           ? static_cast<int>(100 * total_bytes_received / content_length)
                                           : -1;
                        if (!monitor(download_type, progress))
                        {
                            abort_downloads = true;
                            break;
                        }
                    }
                }

                file.close();

                if (abort_downloads)
                {
                    file.remove();
                    throw mp::AbortedDownloadException{"Download aborted"};
                }

                // Download successful
                return;
            }
            else if (response.getStatus() == Poco::Net::HTTPResponse::HTTP_MOVED_PERMANENTLY ||
                     response.getStatus() == Poco::Net::HTTPResponse::HTTP_FOUND ||
                     response.getStatus() == Poco::Net::HTTPResponse::HTTP_SEE_OTHER ||
                     response.getStatus() == Poco::Net::HTTPResponse::HTTP_TEMPORARY_REDIRECT ||
                     response.getStatus() == Poco::Net::HTTPResponse::HTTP_PERMANENT_REDIRECT)
            {
                // Handle redirect
                std::string location = response.get("Location");
                if (location.empty())
                {
                    throw mp::DownloadException{url_str, "Redirect without Location header"};
                }
                uri = Poco::URI(location);
                url_str = uri.toString();
                redirectCount++;
            }
            else
            {
                throw mp::DownloadException{url_str, response.getReason()};
            }
        }

        throw mp::DownloadException{url_str, "Too many redirects when trying to download URL"};
    }
    catch (Poco::Exception& ex)
    {
        throw mp::DownloadException{url_str, ex.displayText()};
    }
}

QByteArray mp::URLDownloader::download(const QUrl& url)
{
    return download(url, false);
}

QByteArray mp::URLDownloader::download(const QUrl& url, bool is_force_update_from_network)
{
    (void)is_force_update_from_network;
    std::string url_str = url.toString().toStdString();

    try
    {
        Poco::URI uri(url_str);
        int maxRedirects = 10;
        int redirectCount = 0;

        while (redirectCount < maxRedirects)
        {
            std::string path = uri.getPathEtc().empty() ? "/" : uri.getPathEtc();
            std::string scheme = uri.getScheme();
            std::unique_ptr<Poco::Net::HTTPClientSession> session;

            if (scheme == "https")
            {
                Poco::Net::Context::Ptr context = new Poco::Net::Context(
                    Poco::Net::Context::CLIENT_USE, "", "", "", Poco::Net::Context::VERIFY_NONE);
                session = std::make_unique<Poco::Net::HTTPSClientSession>(uri.getHost(), uri.getPort(), context);
            }
            else
            {
                session = std::make_unique<Poco::Net::HTTPClientSession>(uri.getHost(), uri.getPort());
            }

            // Create and configure request
            Poco::Net::HTTPRequest request(Poco::Net::HTTPRequest::HTTP_GET, path);
            request.set("Connection", "Keep-Alive");
            request.set("User-Agent",
                        QString::fromStdString(fmt::format("Multipass/{} ({}; {})", multipass::version_string,
                                                           mp::platform::host_version(),
                                                           QSysInfo::currentCpuArchitecture()))
                            .toStdString());

            // Send request and receive response
            session->setTimeout(Poco::Timespan(timeout.count() / 1000, (timeout.count() % 1000) * 1000));
            session->sendRequest(request);
            Poco::Net::HTTPResponse response;
            std::istream& rs = session->receiveResponse(response);

            if (response.getStatus() == Poco::Net::HTTPResponse::HTTP_OK)
            {
                // Read response into QByteArray
                QByteArray data;
                char buffer[8192];

                while (!rs.eof() && !abort_downloads)
                {
                    rs.read(buffer, sizeof(buffer));
                    std::streamsize bytes_read = rs.gcount();
                    if (bytes_read > 0)
                    {
                        data.append(buffer, static_cast<int>(bytes_read));
                    }
                }

                if (abort_downloads)
                {
                    throw mp::AbortedDownloadException{"Download aborted"};
                }

                return data;
            }
            else if (response.getStatus() == Poco::Net::HTTPResponse::HTTP_MOVED_PERMANENTLY ||
                     response.getStatus() == Poco::Net::HTTPResponse::HTTP_FOUND ||
                     response.getStatus() == Poco::Net::HTTPResponse::HTTP_SEE_OTHER ||
                     response.getStatus() == Poco::Net::HTTPResponse::HTTP_TEMPORARY_REDIRECT ||
                     response.getStatus() == Poco::Net::HTTPResponse::HTTP_PERMANENT_REDIRECT)
            {
                // Handle redirect
                std::string location = response.get("Location");
                if (location.empty())
                {
                    throw mp::DownloadException{url_str, "Redirect without Location header"};
                }
                uri = Poco::URI(location);
                url_str = uri.toString();
                redirectCount++;
            }
            else
            {
                throw mp::DownloadException{url_str, response.getReason()};
            }
        }

        throw mp::DownloadException{url_str, "Too many redirects when trying to download URL"};
    }
    catch (Poco::Exception& ex)
    {
        throw mp::DownloadException{url_str, ex.displayText()};
    }
}

QDateTime mp::URLDownloader::last_modified(const QUrl& url)
{
    std::string url_str = url.toString().toStdString();

    try
    {
        Poco::URI uri(url_str);
        int maxRedirects = 10;
        int redirectCount = 0;

        while (redirectCount < maxRedirects)
        {
            std::string path = uri.getPathEtc().empty() ? "/" : uri.getPathEtc();
            std::string scheme = uri.getScheme();
            std::unique_ptr<Poco::Net::HTTPClientSession> session;

            if (scheme == "https")
            {
                Poco::Net::Context::Ptr context = new Poco::Net::Context(
                    Poco::Net::Context::CLIENT_USE, "", "", "", Poco::Net::Context::VERIFY_NONE);
                session = std::make_unique<Poco::Net::HTTPSClientSession>(uri.getHost(), uri.getPort(), context);
            }
            else
            {
                session = std::make_unique<Poco::Net::HTTPClientSession>(uri.getHost(), uri.getPort());
            }

            // Create and send HEAD request
            Poco::Net::HTTPRequest request(Poco::Net::HTTPRequest::HTTP_HEAD, path);
            session->setTimeout(Poco::Timespan(timeout.count() / 1000, (timeout.count() % 1000) * 1000));
            session->sendRequest(request);

            // Receive response
            Poco::Net::HTTPResponse response;
            session->receiveResponse(response);

            if (response.getStatus() == Poco::Net::HTTPResponse::HTTP_OK)
            {
                // Get 'Last-Modified' header
                std::string last_modified_str = response.get("Last-Modified", "");
                if (last_modified_str.empty())
                {
                    return QDateTime();
                }
                else
                {
                    // Parse date string to QDateTime
                    QDateTime last_modified = QDateTime::fromString(QString::fromStdString(last_modified_str),
                                                                    Qt::RFC2822Date);
                    return last_modified;
                }
            }
            else if (response.getStatus() == Poco::Net::HTTPResponse::HTTP_MOVED_PERMANENTLY ||
                     response.getStatus() == Poco::Net::HTTPResponse::HTTP_FOUND ||
                     response.getStatus() == Poco::Net::HTTPResponse::HTTP_SEE_OTHER ||
                     response.getStatus() == Poco::Net::HTTPResponse::HTTP_TEMPORARY_REDIRECT ||
                     response.getStatus() == Poco::Net::HTTPResponse::HTTP_PERMANENT_REDIRECT)
            {
                // Handle redirect
                std::string location = response.get("Location");
                if (location.empty())
                {
                    throw mp::DownloadException{url_str, "Redirect without Location header"};
                }
                uri = Poco::URI(location);
                url_str = uri.toString();
                redirectCount++;
            }
            else
            {
                throw mp::DownloadException{url_str, response.getReason()};
            }
        }

        throw mp::DownloadException{url_str, "Too many redirects when trying to download URL"};
    }
    catch (Poco::Exception& ex)
    {
        throw mp::DownloadException{url_str, ex.displayText()};
    }
}

void mp::URLDownloader::abort_all_downloads()
{
    abort_downloads = true;
}
