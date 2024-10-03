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

#include "lxd_request.h"

#include <multipass/format.h>
#include <multipass/logging/log.h>
#include <multipass/network_access_manager.h>
#include <multipass/version.h>

#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>

namespace mp = multipass;
namespace mpl = multipass::logging;

namespace
{
constexpr auto request_category = "lxd request";

const QJsonObject lxd_request_common(mp::NetworkAccessManager* manager, const std::string& method, QUrl& url,
                                     const QByteArray& data,
                                     const std::vector<std::pair<std::string, Poco::Net::PartSource*>>* parts,
                                     const std::map<std::string, std::string>& headers, int timeout)
{
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

    QByteArray reply_data;
    if (parts)
    {
        // Multipart request
        reply_data = manager->sendMultipartRequest(url, method, *parts, headers);
    }
    else
    {
        // Regular request
        reply_data = manager->sendRequest(url, method, data, headers);
    }

    if (reply_data.isEmpty())
        throw mp::LXDRuntimeError(fmt::format("Empty reply received for {} operation on {}", method, url.toString().toStdString()));

    QJsonParseError json_error;
    auto json_reply = QJsonDocument::fromJson(reply_data, &json_error);

    if (json_error.error != QJsonParseError::NoError)
    {
        std::string error_string{
            fmt::format("Error parsing JSON response for {}: {}", url.toString().toStdString(), json_error.errorString().toStdString())};

        mpl::log(mpl::Level::debug, request_category, fmt::format("{}\n{}", error_string, reply_data.toStdString()));
        throw mp::LXDJsonParseError(error_string);
    }

    if (json_reply.isNull() || !json_reply.isObject())
    {
        std::string error_string{fmt::format("Invalid LXD response for {}", url.toString().toStdString())};

        mpl::log(mpl::Level::debug, request_category, fmt::format("{}\n{}", error_string, reply_data.toStdString()));
        throw mp::LXDJsonParseError(error_string);
    }

    mpl::log(mpl::Level::trace, request_category, fmt::format("Got reply: {}", QString(json_reply.toJson()).toStdString()));

    return json_reply.object();
}
} // namespace

const QJsonObject mp::lxd_request(mp::NetworkAccessManager* manager, const std::string& method, QUrl url,
                                  const std::optional<QJsonObject>& json_data, int timeout)
{
    QByteArray data;
    std::map<std::string, std::string> headers;

    if (json_data)
    {
        data = QJsonDocument(*json_data).toJson(QJsonDocument::Compact);

        headers["Content-Type"] = "application/json";
        headers["Content-Length"] = std::to_string(data.size());

        mpl::log(mpl::Level::trace, request_category, fmt::format("Sending data: {}", data.toStdString()));
    }

    return lxd_request_common(manager, method, url, data, nullptr, headers, timeout);
}

const QJsonObject mp::lxd_request(mp::NetworkAccessManager* manager, const std::string& method, QUrl url,
                                  const std::vector<std::pair<std::string, Poco::Net::PartSource*>>& parts,
                                  int timeout)
{
    std::map<std::string, std::string> headers;
    // Set any headers if necessary

    return lxd_request_common(manager, method, url, QByteArray(), &parts, headers, timeout);
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

        task_reply = lxd_request(manager, "GET", task_url, std::nullopt, timeout);

        if (task_reply["error_code"].toInt() >= 400)
        {
            throw mp::LXDRuntimeError(fmt::format("Error waiting on operation: ({}) {}",
                                                  task_reply["error_code"].toInt(), task_reply["error"].toString().toStdString()));
        }
        else if (task_reply["status_code"].toInt() >= 400)
        {
            throw mp::LXDRuntimeError(fmt::format("Failure waiting on operation: ({}) {}",
                                                  task_reply["status_code"].toInt(), task_reply["status"].toString().toStdString()));
        }
        else if (task_reply["metadata"].toObject()["status_code"].toInt() >= 400)
        {
            throw mp::LXDRuntimeError(fmt::format("Operation completed with error: ({}) {}",
                                                  task_reply["metadata"].toObject()["status_code"].toInt(),
                                                  task_reply["metadata"].toObject()["err"].toString().toStdString()));
        }
    }

    return task_reply;
}
