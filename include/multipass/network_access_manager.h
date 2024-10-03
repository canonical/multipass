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

#ifndef MULTIPASS_NETWORK_ACCESS_MANAGER_H
#define MULTIPASS_NETWORK_ACCESS_MANAGER_H

#include <Poco/Net/HTTPClientSession.h>
#include <Poco/Net/HTTPRequest.h>
#include <Poco/Net/HTTPResponse.h>
#include <Poco/Net/StreamSocket.h>
#include <Poco/Net/HTMLForm.h>
#include <Poco/Net/PartSource.h>

#include <QString>
#include <QUrl>
#include <QByteArray>
#include <map>
#include <vector>
#include <memory>

namespace multipass
{

class NetworkAccessManager
{
public:
    using UPtr = std::unique_ptr<NetworkAccessManager>;
    NetworkAccessManager();
    ~NetworkAccessManager();

    QByteArray sendRequest(const QUrl& url, const std::string& method, const QByteArray& data = QByteArray(),
                           const std::map<std::string, std::string>& headers = {});

    // New method for multipart requests
    QByteArray sendMultipartRequest(const QUrl& url, const std::string& method,
                                    const std::vector<std::pair<std::string, Poco::Net::PartSource*>>& parts,
                                    const std::map<std::string, std::string>& headers = {});

private:
    QByteArray sendUnixRequest(const QUrl& url, const std::string& method, const QByteArray& data,
                               const std::map<std::string, std::string>& headers);

    QByteArray sendUnixMultipartRequest(const QUrl& url, const std::string& method,
                                        const std::vector<std::pair<std::string, Poco::Net::PartSource*>>& parts,
                                        const std::map<std::string, std::string>& headers);
};

} // namespace multipass

#endif // MULTIPASS_NETWORK_ACCESS_MANAGER_H
