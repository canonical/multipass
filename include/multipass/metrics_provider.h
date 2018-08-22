/*
 * Copyright (C) 2018 Canonical, Ltd.
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

#ifndef MULTIPASS_METRICS_PROVIDER_H
#define MULTIPASS_METRICS_PROVIDER_H

#include <multipass/auto_join_thread.h>
#include <multipass/path.h>

#include <QByteArray>
#include <QJsonArray>
#include <QNetworkAccessManager>
#include <QString>
#include <QUrl>

#include <condition_variable>
#include <mutex>

namespace multipass
{
class MetricsProvider
{
public:
    MetricsProvider(const QUrl& url, const QString& unique_id, const Path& path);
    MetricsProvider(const QString& metrics_url, const QString& unique_id, const Path& path);
    ~MetricsProvider();

    bool send_metrics();
    void send_denied();

private:
    void update_and_notify_sender(const QJsonObject& metric);

    const QUrl metrics_url;
    const QString unique_id;
    const Path& data_path;
    QJsonArray metric_batches;

    std::mutex metrics_mutex;
    std::condition_variable metrics_cv;
    bool running{true};
    bool metrics_available{false};
    AutoJoinThread metrics_sender;
};
} // namespace multipass
#endif // MULTIPASS_METRICS_PROVIDER_H
