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

#ifndef MULTIPASS_MOCK_URL_DOWNLOADER_H
#define MULTIPASS_MOCK_URL_DOWNLOADER_H

#include <multipass/url_downloader.h>

namespace multipass
{
namespace test
{
struct MockURLDownloader : public multipass::URLDownloader
{
    MockURLDownloader() : URLDownloader{std::chrono::seconds(10)} {};

    MOCK_METHOD1(download, QByteArray(const QUrl&));
    MOCK_METHOD1(last_modified, QDateTime(const QUrl&));
    MOCK_METHOD5(download_to, void(const QUrl&, const QString&, int64_t, const int, const ProgressMonitor&));
};
} // namespace test
} // namespace multipass
#endif // MULTIPASS_MOCK_URL_DOWNLOADER_H
