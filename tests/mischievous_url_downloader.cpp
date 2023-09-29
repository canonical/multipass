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

#include "mischievous_url_downloader.h"

namespace mp = multipass;
namespace mpt = multipass::test;

mpt::MischievousURLDownloader::MischievousURLDownloader(std::chrono::milliseconds timeout) : URLDownloader(timeout)
{
}

void mpt::MischievousURLDownloader::download_to(const QUrl& url, const QString& file_name, int64_t size,
                                                const int download_type, const mp::ProgressMonitor& monitor)
{
    URLDownloader::download_to(choose_url(url), file_name, size, download_type, monitor);
}

QByteArray mpt::MischievousURLDownloader::download(const QUrl& url)
{
    return URLDownloader::download(choose_url(url));
}

QByteArray mpt::MischievousURLDownloader::download(const QUrl& url, const bool is_force_update_from_network)
{
    return URLDownloader::download(choose_url(url), is_force_update_from_network);
}

QDateTime mpt::MischievousURLDownloader::last_modified(const QUrl& url)
{
    return URLDownloader::last_modified(choose_url(url));
}

const QUrl& mpt::MischievousURLDownloader::choose_url(const QUrl& url)
{
    return mischiefs-- > 0 ? empty_url : url;
}
