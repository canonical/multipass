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

#ifndef MULTIPASS_LXD_REQUEST_H
#define MULTIPASS_LXD_REQUEST_H

#include <QHttpMultiPart>
#include <QJsonObject>
#include <QUrl>

#include <optional>
#include <string>

namespace multipass
{
const QUrl lxd_socket_url{"unix:///var/snap/lxd/common/lxd/unix.socket@1.0"};
const QString lxd_project_name{"multipass"};

class NetworkAccessManager;

class LXDNotFoundException : public std::runtime_error
{
public:
    LXDNotFoundException() : runtime_error{"LXD object not found"}
    {
    }
};

class LXDRuntimeError : public std::runtime_error
{
public:
    LXDRuntimeError(const std::string& message) : runtime_error{message}
    {
    }
};

class LXDNetworkError : public LXDRuntimeError
{
public:
    LXDNetworkError(const std::string& message) : LXDRuntimeError{message}
    {
    }
};

class LXDJsonParseError : public std::runtime_error
{
public:
    LXDJsonParseError(const std::string& message) : runtime_error{message}
    {
    }
};

const QJsonObject lxd_request(NetworkAccessManager* manager, const std::string& method, QUrl url,
                              const std::optional<QJsonObject>& json_data = std::nullopt,
                              int timeout = 30000 /* in milliseconds */);

const QJsonObject lxd_request(NetworkAccessManager* manager, const std::string& method, QUrl url,
                              QHttpMultiPart& multi_part, int timeout = 30000 /* in milliseconds */);

const QJsonObject lxd_wait(NetworkAccessManager* manager, const QUrl& base_url, const QJsonObject& task_data,
                           int timeout /* in milliseconds */);
} // namespace multipass

#endif // MULTIPASS_LXD_REQUEST_H
