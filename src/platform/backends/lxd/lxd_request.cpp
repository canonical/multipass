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

void log_and_throw_error(const std::string& message, mpl::Level level)
{
    mpl::log(level, request_category, message);

    throw std::runtime_error(message);
}

template <typename Callable>
const QJsonObject lxd_request_common(const std::string& method, QUrl& url, int timeout, Callable&& handle_request)
{
    QEventLoop event_loop;
    QTimer download_timeout;
    download_timeout.setInterval(timeout);

    if (url.host().isEmpty())
    {
        url.setHost(mp::lxd_project_name);
    }

    const QString project_query_string = QString("project=%1").arg(mp::lxd_project_name);
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

    auto reply = handle_request(request, verb);

    QObject::connect(reply, &QNetworkReply::finished, &event_loop, &QEventLoop::quit);
    QObject::connect(&download_timeout, &QTimer::timeout, [&]() {
        download_timeout.stop();
        reply->abort();
    });

    if (!reply->isFinished())
    {
        download_timeout.start();
        event_loop.exec();
    }

    if (reply->error() == QNetworkReply::ContentNotFoundError)
        throw mp::LXDNotFoundException();

    if (reply->error() == QNetworkReply::OperationCanceledError)
        log_and_throw_error(fmt::format("Timeout getting response for {} operation on {}", method, url.toString()),
                            mpl::Level::warning);

    auto bytearray_reply = reply->readAll();
    reply->deleteLater();

    if (bytearray_reply.isEmpty())
        log_and_throw_error(fmt::format("Empty reply received for {} operation on {}", method, url.toString()),
                            mpl::Level::error);

    QJsonParseError json_error;
    auto json_reply = QJsonDocument::fromJson(bytearray_reply, &json_error);

    if (json_error.error != QJsonParseError::NoError)
        log_and_throw_error(fmt::format("Error parsing JSON response for {}: {}\n{}", url.toString(),
                                        json_error.errorString(), bytearray_reply),
                            mpl::Level::error);

    if (json_reply.isNull() || !json_reply.isObject())
        log_and_throw_error(fmt::format("Invalid LXD response for {}: {}", url.toString(), bytearray_reply),
                            mpl::Level::error);

    mpl::log(mpl::Level::trace, request_category, fmt::format("Got reply: {}", QJsonDocument(json_reply).toJson()));

    if (reply->error() != QNetworkReply::NoError)
        log_and_throw_error(fmt::format("Network error for {}: {} - {}", url.toString(), reply->errorString(),
                                        json_reply.object()["error"].toString()),
                            mpl::Level::error);

    return json_reply.object();
}
} // namespace

const QJsonObject mp::lxd_request(mp::NetworkAccessManager* manager, const std::string& method, QUrl url,
                                  const mp::optional<QJsonObject>& json_data, int timeout)
{
    auto handle_request = [manager, &json_data](QNetworkRequest& request, const QByteArray& verb) {
        QByteArray data;
        if (json_data)
        {
            data = QJsonDocument(*json_data).toJson(QJsonDocument::Compact);

            request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
            request.setHeader(QNetworkRequest::ContentLengthHeader, QByteArray::number(data.size()));

            mpl::log(mpl::Level::trace, request_category, fmt::format("Sending data: {}", data));
        }

        return manager->sendCustomRequest(request, verb, data);
    };

    return lxd_request_common(method, url, timeout, handle_request);
}

const QJsonObject mp::lxd_request(mp::NetworkAccessManager* manager, const std::string& method, QUrl url,
                                  QHttpMultiPart& multi_part, int timeout)
{
    auto handle_request = [manager, &multi_part](QNetworkRequest& request, const QByteArray& verb) {
        request.setRawHeader("Transfer-Encoding", "chunked");

        return manager->sendCustomRequest(request, verb, &multi_part);
    };

    return lxd_request_common(method, url, timeout, handle_request);
}

const QJsonObject mp::lxd_wait(mp::NetworkAccessManager* manager, const QUrl& base_url, const QJsonObject& task_data,
                               int timeout)
{
    QJsonObject task_reply;

    if (task_data["metadata"].toObject()["class"] == QStringLiteral("task") &&
        task_data["status_code"].toInt(-1) == 100)
    {
        QUrl task_url(QString("%1/operations/%2/wait")
                          .arg(base_url.toString())
                          .arg(task_data["metadata"].toObject()["id"].toString()));

        task_reply = lxd_request(manager, "GET", task_url, mp::nullopt, timeout);

        if (task_reply["error_code"].toInt() >= 400)
        {
            log_and_throw_error(fmt::format("Error waiting on operation: {}", task_reply["error"].toString()),
                                mpl::Level::error);
        }
        else if (task_reply["status_code"].toInt() == 400)
        {
            log_and_throw_error(fmt::format("Failure waiting on operation: {}", task_reply["status"].toString()),
                                mpl::Level::error);
        }
        else if (task_reply["metadata"].toObject()["status_code"].toInt() == 400)
        {
            log_and_throw_error(
                fmt::format("Operation completed with error: {}", task_reply["metadata"].toObject()["err"].toString()),
                mpl::Level::error);
        }
    }

    return task_reply;
}
