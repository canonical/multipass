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

#include "local_socket_reply.h"

#include <multipass/exceptions/local_socket_connection_exception.h>
#include <multipass/format.h>
#include <multipass/network_access_manager.h>

namespace mp = multipass;

mp::NetworkAccessManager::NetworkAccessManager(QObject* parent) : QNetworkAccessManager(parent)
{
}

QNetworkReply* mp::NetworkAccessManager::createRequest(QNetworkAccessManager::Operation operation,
                                                       const QNetworkRequest& orig_request, QIODevice* device)
{
    auto scheme = orig_request.url().scheme();

    // To support http requests over Unix sockets, the initial URL needs to be in the form of:
    // unix:///path/to/unix_socket@path/in/server (or 'local' instead of 'unix')
    //
    // For example, to get the general LXD configuration when LXD is installed as a snap:
    // unix:////var/snap/lxd/common/lxd/unix.socket@1.0
    if (scheme == "unix" || scheme == "local")
    {
        const auto url_parts = orig_request.url().toString().split('@');
        if (url_parts.count() != 2)
        {
            throw LocalSocketConnectionException("The local socket scheme is malformed.");
        }

        const auto socket_path = QUrl(url_parts[0]).path();

        LocalSocketUPtr local_socket = std::make_unique<QLocalSocket>();

        local_socket->connectToServer(socket_path);
        if (!local_socket->waitForConnected(5000))
        {
            throw LocalSocketConnectionException(
                fmt::format("Cannot connect to {}: {}", socket_path, local_socket->errorString()));
        }

        const auto server_path = url_parts[1];
        QNetworkRequest request{orig_request};

        QUrl url(QString("/%1").arg(server_path));
        url.setHost(orig_request.url().host());

        request.setUrl(url);

        // The caller needs to be responsible for freeing the allocated memory
        return new LocalSocketReply(std::move(local_socket), request, device);
    }
    else
    {
        return QNetworkAccessManager::createRequest(operation, orig_request, device);
    }
}
