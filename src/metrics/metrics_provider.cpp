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

#include <multipass/logging/log.h>
#include <multipass/metrics_provider.h>
#include <multipass/utils.h>

#include <fmt/format.h>

#include <QByteArray>
#include <QDateTime>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegExp>
#include <QString>
#include <QUuid>

namespace mp = multipass;
namespace mpl = multipass::logging;

namespace
{
constexpr auto category = "metrics";
}

mp::MetricsProvider::MetricsProvider(const QUrl& metrics_url, const QString& unique_id)
    : metrics_url{metrics_url}, unique_id{unique_id}
{
}

mp::MetricsProvider::MetricsProvider(const QString& metrics_url, const QString& unique_id)
    : MetricsProvider{QUrl{metrics_url}, unique_id}
{
}

bool mp::MetricsProvider::send_metrics()
{
    QJsonObject tags;
    tags.insert("multipass_id", unique_id);

    QJsonObject metric;
    metric.insert("key", "host-machine-info");
    metric.insert("value", "1");
    metric.insert("time", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    metric.insert("tags", tags);

    QJsonArray metrics;
    metrics.push_back(metric);

    QJsonObject metric_batch;
    metric_batch.insert("uuid", generate_unique_id());
    metric_batch.insert("created", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    metric_batch.insert("metrics", metrics);
    metric_batch.insert("credentials", QString());

    QJsonArray metric_batches;
    metric_batches.push_back(metric_batch);
    auto body = QJsonDocument(metric_batches).toJson(QJsonDocument::Compact);

    post_request(body);

    return true;
}

void mp::MetricsProvider::send_denied()
{
    QJsonObject denied;
    denied.insert("denied", 1);

    QJsonArray denied_batches;
    denied_batches.push_back(denied);

    auto body = QJsonDocument(denied_batches).toJson(QJsonDocument::Compact);

    post_request(body);
}

QString mp::MetricsProvider::generate_unique_id()
{
    auto uuid = QUuid::createUuid();
    return uuid.toString().remove(QRegExp("[{}]"));
}

void mp::MetricsProvider::post_request(const QByteArray& body)
{
    QNetworkRequest request{metrics_url};
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setHeader(QNetworkRequest::ContentLengthHeader, body.size());
    request.setHeader(QNetworkRequest::UserAgentHeader, "multipassd/1.0");

    QEventLoop event_loop;
    std::unique_ptr<QNetworkReply> reply;

    // For unit testing
    if (metrics_url.isLocalFile())
        reply = std::unique_ptr<QNetworkReply>(manager.put(request, body));
    else
        reply = std::unique_ptr<QNetworkReply>(manager.post(request, body));

    QObject::connect(reply.get(), &QNetworkReply::finished, &event_loop, &QEventLoop::quit);

    event_loop.exec();

    auto buff = reply->readAll();

    if (reply->error() != QNetworkReply::NoError)
    {
        auto error_msg = QJsonDocument::fromJson(buff).object();
        mpl::log(mpl::Level::error, category,
                 fmt::format("Metrics error: {} - {}\n", error_msg["code"].toString().toStdString(),
                             error_msg["message"].toString().toStdString()));
    }
}
