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

#include <QByteArray>
#include <QNetworkAccessManager>
#include <QString>
#include <QUrl>

namespace multipass
{
class MetricsProvider
{
public:
    MetricsProvider(const QUrl& metrics_url, const QString& unique_id);
    MetricsProvider(const QString& metrics_url, const QString& unique_id);

    bool send_metrics();
    void send_denied();

private:
    void post_request(const QByteArray& body);

    const QUrl metrics_url;
    const QString unique_id;
    QNetworkAccessManager manager;
};
} // namespace multipass
#endif // MULTIPASS_METRICS_PROVIDER_H
