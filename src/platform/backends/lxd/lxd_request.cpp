/*
 * Copyright (C) 2020 Canonical, Ltd.
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

#include "lxd_request.h"

#include <multipass/format.h>
#include <multipass/logging/log.h>
#include <multipass/network_access_manager.h>
#include <multipass/version.h>

#include <QEventLoop>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QTimer>

namespace mp = multipass;
namespace mpl = multipass::logging;

namespace
{
constexpr auto request_category = "lxd request";
} // namespace

const QJsonObject mp::lxd_request(mp::NetworkAccessManager* manager, const std::string& method, QUrl url,
                                  const mp::optional<QJsonObject>& json_data, int timeout)
{
    QEventLoop event_loop;
    QTimer download_timeout;
    download_timeout.setInterval(timeout);

    if (url.host().isEmpty())
    {
        url.setHost(lxd_project_name);
    }

    const QString project_query_string = QString("project=%1").arg(lxd_project_name);
    if (url.hasQuery())
    {
        url.setQuery(url.query() + "&" + project_query_string);
    }
    else
    {
        url.setQuery(project_query_string);
    }

    mpl::log(mpl::Level::trace, request_category, fmt::format("Requesting LXD: {} {}", method, url.toString()));
    QNetworkRequest request{url};

    request.setHeader(QNetworkRequest::UserAgentHeader, QString("Multipass/%1").arg(mp::version_string));

    auto verb = QByteArray::fromStdString(method);
    QByteArray data;
    if (json_data)
    {
        data = QJsonDocument(*json_data).toJson(QJsonDocument::Compact);

        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
        request.setHeader(QNetworkRequest::ContentLengthHeader, QByteArray::number(data.size()));

        mpl::log(mpl::Level::trace, request_category, fmt::format("Sending data: {}", data));
    }

    auto reply = manager->sendCustomRequest(request, verb, data);

    QObject::connect(reply, &QNetworkReply::finished, &event_loop, &QEventLoop::quit);
    QObject::connect(&download_timeout, &QTimer::timeout, [&]() {
        mpl::log(mpl::Level::warning, request_category,
                 fmt::format("Request timed out: {} {}", method, url.toString()));
        download_timeout.stop();
        reply->abort();
    });

    if (!reply->isFinished())
    {
        download_timeout.start();
        event_loop.exec();
    }

    if (reply->error() == QNetworkReply::ContentNotFoundError)
        throw LXDNotFoundException();

    if (reply->error() != QNetworkReply::NoError)
        throw std::runtime_error(fmt::format("{}: {}", url.toString(), reply->errorString()));

    auto bytearray_reply = reply->readAll();
    reply->deleteLater();

    QJsonParseError json_error;
    auto json_reply = QJsonDocument::fromJson(bytearray_reply, &json_error);

    if (json_error.error != QJsonParseError::NoError)
        throw std::runtime_error(fmt::format("{}: {}", url.toString(), json_error.errorString()));

    if (json_reply.isNull() || !json_reply.isObject())
        throw std::runtime_error(fmt::format("Invalid LXD response for url {}: {}", url.toString(), bytearray_reply));

    mpl::log(mpl::Level::trace, request_category, fmt::format("Got reply: {}", QJsonDocument(json_reply).toJson()));

    return json_reply.object();
}
