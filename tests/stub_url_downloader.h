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

#ifndef MULTIPASS_STUB_URL_DOWNLOADER_H
#define MULTIPASS_STUB_URL_DOWNLOADER_H

#include <multipass/url_downloader.h>

namespace multipass
{
namespace test
{
struct StubURLDownloader : public multipass::URLDownloader
{
    StubURLDownloader() : multipass::URLDownloader{std::chrono::seconds(10)}
    {
    }
    void download_to(const QUrl& url, const QString& file_name, int64_t size, const int download_type,
                     const multipass::ProgressMonitor&) override
    {
    }
    QByteArray download(const QUrl& url) override
    {
        return {};
    }
};
} // namespace test
} // namespace multipass
#endif // MULTIPASS_STUB_URL_DOWNLOADER_H
