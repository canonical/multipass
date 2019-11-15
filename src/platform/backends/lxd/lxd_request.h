/*
 * Copyright (C) 2019 Canonical, Ltd.
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

#ifndef MULTIPASS_LXD_REQUEST_H
#define MULTIPASS_LXD_REQUEST_H

#include <QFile>
#include <QEventLoop>
#include <QBuffer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkRequest>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QSslCertificate>
#include <QtNetwork/QSslConfiguration>
#include <QtNetwork/QSslKey>

#include <multipass/optional.h>
#include <multipass/format.h>
#include <multipass/logging/log.h>

#include <QDebug>

namespace mp = multipass;
namespace mpl = multipass::logging;

namespace
{
typedef std::exception LXDNotFoundException;

constexpr auto request_category = "lxd request";

auto make_ssl_config()
{
    QSslConfiguration ssl_config;
    ssl_config.setLocalCertificate(QSslCertificate::fromPath("/home/ubuntu/.config/lxc/client.crt")[0]);
    QFile keyfile{"/home/ubuntu/.config/lxc/client.key"};
    keyfile.open(QIODevice::ReadOnly);
    ssl_config.setPrivateKey(QSslKey(keyfile.readAll(), QSsl::Rsa));
    ssl_config.setPeerVerifyMode(QSslSocket::VerifyNone);

    return ssl_config;
}

const QJsonObject lxd_request(QNetworkAccessManager* manager, std::string method, QUrl url, const mp::optional<QJsonObject>& json_data = mp::nullopt, int timeout = 5000, bool wait = true)
{
    mpl::log(mpl::Level::debug, request_category, fmt::format("Requesting LXD: {} {}", method, url.toString()));

    QEventLoop event_loop;
    QTimer download_timeout;
    download_timeout.setInterval(timeout);

    url.setQuery("project=multipass");

    QNetworkRequest request{url};
    request.setSslConfiguration(make_ssl_config());

    auto verb = QByteArray::fromStdString(method);
    QByteArray data;
    if (json_data)
    {
        data = QJsonDocument(*json_data).toJson();
        mpl::log(mpl::Level::debug, request_category, fmt::format("Sending data: {}", data));
    }

    auto reply = manager->sendCustomRequest(request, verb, data);

    QObject::connect(reply, &QNetworkReply::finished, &event_loop, &QEventLoop::quit);
    QObject::connect(&download_timeout, &QTimer::timeout, [&]() {
        mpl::log(mpl::Level::debug, request_category, fmt::format("Request timed out: {} {}", method, url.toString()));
        download_timeout.stop();
        reply->abort();
    });

    download_timeout.start();
    event_loop.exec();

    if (reply->error() == QNetworkReply::ContentNotFoundError)
        throw LXDNotFoundException();

    if (reply->error() != QNetworkReply::NoError)
        throw std::runtime_error(fmt::format("{}: {}", url.toString(), reply->errorString()));

    auto bytearray_reply = reply->readAll();
    QJsonParseError json_error;
    auto json_reply = QJsonDocument::fromJson(bytearray_reply, &json_error);

    if (json_error.error != QJsonParseError::NoError)
        throw std::runtime_error(fmt::format("{}: {}", url.toString(), json_error.errorString()));

    if (json_reply.isNull() || !json_reply.isObject())
        throw std::runtime_error(fmt::format("Invalid LXD response for url {}: {}", url.toString(), bytearray_reply));

    mpl::log(mpl::Level::debug, request_category, fmt::format("Got reply: {}", QJsonDocument(json_reply).toJson()));

    if (wait && json_reply.object()["metadata"].toObject()["class"] == QStringLiteral("task") && json_reply.object()["status_code"].toInt(-1) == 100)
    {
        const auto task = json_reply.object()["metadata"].toObject();
        QUrl task_url(url);
        task_url.setPath(QString("%1/wait").arg(json_reply.object()["operation"].toString()));

        mpl::log(mpl::Level::debug, request_category, fmt::format("Got LXD task \"{}\": \"{}\", waiting...", task["id"].toString(), task["description"].toString()));
        return lxd_request(manager, "GET", task_url, mp::nullopt, 60000);
    }

    return json_reply.object();
}
} //namespace

#endif // MULTIPASS_LXD_REQUEST_H
