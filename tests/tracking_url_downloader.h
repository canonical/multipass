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

#ifndef MULTIPASS_TRACKING_URL_DOWNLOADER_H
#define MULTIPASS_TRACKING_URL_DOWNLOADER_H

#include "file_operations.h"

#include <multipass/url_downloader.h>

namespace multipass
{
namespace test
{
struct TrackingURLDownloader : public URLDownloader
{
    TrackingURLDownloader(const std::string& content) : URLDownloader{std::chrono::seconds(10)}, content{content}
    {
    }

    TrackingURLDownloader() : TrackingURLDownloader{""}
    {
    }

    void download_to(const QUrl& url, const QString& file_name, int64_t size, const int download_type,
                     const ProgressMonitor&) override
    {
        make_file_with_content(file_name, content);
        downloaded_urls << url.toString();
        downloaded_files << file_name;
    }

    QByteArray download(const QUrl& url) override
    {
        return {};
    }

    QDateTime last_modified(const QUrl& url) override
    {
        return QDateTime::currentDateTime();
    }

    const std::string content;
    QStringList downloaded_files;
    QStringList downloaded_urls;
};
} // namespace test
} // namespace multipass

#endif // MULTIPASS_TRACKING_URL_DOWNLOADER_H
