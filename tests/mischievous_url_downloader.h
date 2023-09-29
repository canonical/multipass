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

#ifndef MULTIPASS_MISCHIEVOUS_URL_DOWNLOADER_H
#define MULTIPASS_MISCHIEVOUS_URL_DOWNLOADER_H

#include <multipass/url_downloader.h>

#include <QUrl>

namespace multipass
{
namespace test
{
class MischievousURLDownloader : public URLDownloader
{
public:
    MischievousURLDownloader(std::chrono::milliseconds timeout);

    void download_to(const QUrl& url, const QString& file_name, int64_t size, const int download_type,
                     const ProgressMonitor& monitor) override;
    QByteArray download(const QUrl& url) override;
    QByteArray download(const QUrl& url, const bool is_force_update_from_network) override;
    QDateTime last_modified(const QUrl& url) override;

public:
    int mischiefs = 0;

private:
    const QUrl& choose_url(const QUrl& url);

    QUrl empty_url = {};
};
} // namespace test
} // namespace multipass

#endif /* MULTIPASS_MISCHIEVOUS_URL_DOWNLOADER_H */
