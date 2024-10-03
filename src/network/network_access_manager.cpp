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

#include <multipass/network_access_manager.h>
#include <Poco/Net/SocketAddress.h>
#include <Poco/Exception.h>
#include <QUrl>
#include <QByteArray>

namespace mp = multipass;

mp::NetworkAccessManager::NetworkAccessManager()
{
}

mp::NetworkAccessManager::~NetworkAccessManager()
{
}

QByteArray mp::NetworkAccessManager::sendRequest(const QUrl& url, const std::string& method, const QByteArray& data,
                                                 const std::map<std::string, std::string>& headers)
{
    auto scheme = url.scheme();

    if (scheme == "unix" || scheme == "local")
    {
        return sendUnixRequest(url, method, data, headers);
    }
    else
    {
        throw std::runtime_error("Only UNIX socket requests are supported");
    }
}

QByteArray mp::NetworkAccessManager::sendMultipartRequest(const QUrl& url, const std::string& method,
                                                          const std::vector<std::pair<std::string, Poco::Net::PartSource*>>& parts,
                                                          const std::map<std::string, std::string>& headers)
{
    auto scheme = url.scheme();

    if (scheme == "unix" || scheme == "local")
    {
        return sendUnixMultipartRequest(url, method, parts, headers);
    }
    else
    {
        throw std::runtime_error("Only UNIX socket requests are supported");
    }
}

QByteArray mp::NetworkAccessManager::sendUnixRequest(const QUrl& url, const std::string& method, const QByteArray& data,
                                                     const std::map<std::string, std::string>& headers)
{
    // Parse the URL to get the socket path and the request path
    auto url_str = url.toString();
    auto url_parts = url_str.split('@');
    if (url_parts.count() != 2)
    {
        throw std::runtime_error("The local socket scheme is malformed.");
    }

    auto socket_path = QUrl(url_parts[0]).path().toStdString();
    auto request_path = url_parts[1].toStdString();

    try
    {
        // Create a local stream socket and connect to the UNIX socket
        Poco::Net::SocketAddress local_address(socket_path);
        Poco::Net::StreamSocket local_socket;
        local_socket.connect(local_address);

        // Create an HTTP client session with the local socket
        Poco::Net::HTTPClientSession session(local_socket);

        // Create the request
        Poco::Net::HTTPRequest request(method, "/" + request_path, Poco::Net::HTTPMessage::HTTP_1_1);
        if (!data.isEmpty())
            request.setContentLength(data.size());

        // Set headers
        for (const auto& header : headers)
        {
            request.set(header.first, header.second);
        }

        // Send the request
        std::ostream& os = session.sendRequest(request);
        if (!data.isEmpty())
        {
            os.write(data.constData(), data.size());
        }

        // Receive the response
        Poco::Net::HTTPResponse response;
        std::istream& rs = session.receiveResponse(response);

        // Read the response data
        QByteArray response_data;
        char buffer[1024];
        while (rs.read(buffer, sizeof(buffer)))
        {
            response_data.append(buffer, rs.gcount());
        }
        // Read any remaining bytes
        if (rs.gcount() > 0)
        {
            response_data.append(buffer, rs.gcount());
        }

        return response_data;
    }
    catch (Poco::Exception& ex)
    {
        throw std::runtime_error("Failed to communicate over UNIX socket: " + ex.displayText());
    }
}

QByteArray mp::NetworkAccessManager::sendUnixMultipartRequest(const QUrl& url, const std::string& method,
                                                              const std::vector<std::pair<std::string, Poco::Net::PartSource*>>& parts,
                                                              const std::map<std::string, std::string>& headers)
{
    // Parse the URL to get the socket path and the request path
    auto url_str = url.toString();
    auto url_parts = url_str.split('@');
    if (url_parts.count() != 2)
    {
        throw std::runtime_error("The local socket scheme is malformed.");
    }

    auto socket_path = QUrl(url_parts[0]).path().toStdString();
    auto request_path = url_parts[1].toStdString();

    try
    {
        // Create a local stream socket and connect to the UNIX socket
        Poco::Net::SocketAddress local_address(socket_path);
        Poco::Net::StreamSocket local_socket;
        local_socket.connect(local_address);

        // Create an HTTP client session with the local socket
        Poco::Net::HTTPClientSession session(local_socket);

        // Create the request
        Poco::Net::HTTPRequest request(method, "/" + request_path, Poco::Net::HTTPMessage::HTTP_1_1);

        // Set headers
        for (const auto& header : headers)
        {
            request.set(header.first, header.second);
        }

        // Create the HTMLForm and add parts
        Poco::Net::HTMLForm form(Poco::Net::HTMLForm::ENCODING_MULTIPART);
        for (const auto& part : parts)
        {
            form.addPart(part.first, part.second); // HTMLForm takes ownership of PartSource*
        }

        // Prepare the request with the form
        form.prepareSubmit(request);

        // Send the request
        std::ostream& os = session.sendRequest(request);
        form.write(os);

        // Receive the response
        Poco::Net::HTTPResponse response;
        std::istream& rs = session.receiveResponse(response);

        // Read the response data
        QByteArray response_data;
        char buffer[1024];
        while (rs.read(buffer, sizeof(buffer)))
        {
            response_data.append(buffer, rs.gcount());
        }
        // Read any remaining bytes
        if (rs.gcount() > 0)
        {
            response_data.append(buffer, rs.gcount());
        }

        return response_data;
    }
    catch (Poco::Exception& ex)
    {
        throw std::runtime_error("Failed to communicate over UNIX socket: " + ex.displayText());
    }
}
