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
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkRequest>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QSslCertificate>
#include <QtNetwork/QSslConfiguration>
#include <QtNetwork/QSslKey>

#include <multipass/format.h>
#include <multipass/logging/log.h>

#include <QDebug>

namespace mpl = multipass::logging;

namespace
{
typedef std::exception LXDNotFoundException;

auto make_network_manager()
{
    return std::make_unique<QNetworkAccessManager>();
}

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

const auto lxd_request(QNetworkAccessManager* manager, std::string method, QUrl const& url)
{
    mpl::log(mpl::Level::debug, "lxd request", fmt::format("Requesting LXD: {} {}", method, url.toString()));

    QEventLoop event_loop;
    QTimer download_timeout;
    download_timeout.setInterval(5000);

    QNetworkRequest request{url};
    request.setSslConfiguration(make_ssl_config());

    auto verb = QByteArray::fromStdString(method);
    auto reply = manager->sendCustomRequest(request, verb);

    QObject::connect(reply, &QNetworkReply::finished, &event_loop, &QEventLoop::quit);
    QObject::connect(&download_timeout, &QTimer::timeout, [&]() {
        mpl::log(mpl::Level::debug, "lxd request", fmt::format("Request timed out: {} {}", method, url.toString()));
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

    return json_reply.object();
}
} //namespace

#endif // MULTIPASS_LXD_REQUEST_H
