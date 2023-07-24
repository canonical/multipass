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

#include <multipass/exceptions/http_local_socket_exception.h>
#include <multipass/format.h>

#include <vector>

#include <QRegularExpression>

namespace mp = multipass;

namespace
{
constexpr int len = 65536;
constexpr int max_bytes = 32768;

// Status code mapping based on
// https://github.com/qt/qtbase/blob/dev/src/network/access/qhttpthreaddelegate.cpp
QNetworkReply::NetworkError statusCodeFromHttp(int httpStatusCode)
{
    QNetworkReply::NetworkError code;

    // Only switch on the LXD HTTP errors:
    // https://lxd.readthedocs.io/en/latest/rest-api/#error
    switch (httpStatusCode)
    {
    case 400: // Bad Request
        code = QNetworkReply::ProtocolInvalidOperationError;
        break;

    case 401: // Authorization required
        code = QNetworkReply::AuthenticationRequiredError;
        break;

    case 403: // Access denied
        code = QNetworkReply::ContentAccessDenied;
        break;

    case 404: // Not Found
        code = QNetworkReply::ContentNotFoundError;
        break;

    case 409: // Resource Conflict
        code = QNetworkReply::ContentConflictError;
        break;

    case 500: // Internal Server Error
        code = QNetworkReply::InternalServerError;
        break;

    default:
        if (httpStatusCode > 500)
        {
            // some kind of server error
            code = QNetworkReply::UnknownServerError;
        }
        else
        {
            // content error we did not handle above
            code = QNetworkReply::UnknownContentError;
        }
    }

    return code;
}
} // namespace

mp::LocalSocketReply::LocalSocketReply(LocalSocketUPtr local_socket, const QNetworkRequest& request,
                                       QIODevice* outgoingData)
    : QNetworkReply(), local_socket{std::move(local_socket)}, reply_data{QByteArray(len, '\0')}
{
    open(QIODevice::ReadOnly);

    QObject::connect(this->local_socket.get(), &QLocalSocket::readyRead, this, &LocalSocketReply::read_reply);
    QObject::connect(this->local_socket.get(), &QLocalSocket::readChannelFinished, this,
                     &LocalSocketReply::read_finish);

    send_request(request, outgoingData);
}

// Mainly for testing
mp::LocalSocketReply::LocalSocketReply()
{
    open(QIODevice::ReadOnly);

    setFinished(true);
    emit finished();
}

mp::LocalSocketReply::~LocalSocketReply()
{
    if (local_socket)
    {
        local_socket->disconnectFromServer();
    }
}

void mp::LocalSocketReply::abort()
{
    close();

    setError(OperationCanceledError, "Operation canceled");
    emit errorOccurred(OperationCanceledError);

    setFinished(true);
    emit finished();
}

qint64 mp::LocalSocketReply::readData(char* data, qint64 maxSize)
{
    if (offset < content_data.size())
    {
        qint64 number = qMin(maxSize, content_data.size() - offset);
        memcpy(data, content_data.constData() + offset, number);
        offset += number;

        return number;
    }

    return -1;
}

void mp::LocalSocketReply::send_request(const QNetworkRequest& request, QIODevice* outgoingData)
{
    QByteArray http_data;
    http_data.reserve(1024);

    auto op = request.attribute(QNetworkRequest::CustomVerbAttribute).toByteArray();

    // Build the HTTP method part
    http_data += op + ' ' + request.url().path().toLatin1();

    if (request.url().hasQuery())
    {
        http_data += "?" + request.url().query().toLatin1();
    }

    http_data += " HTTP/1.1\r\n";

    // Build the HTTP Host header
    auto host = request.url().host().toLatin1();
    if (!host.isEmpty())
    {
        http_data += "Host: " + host + "\r\n";
    }
    else
    {
        throw mp::HttpLocalSocketException("Host is required in URL");
    }

    // Build the HTTP User-Agent header
    auto user_agent = request.header(QNetworkRequest::UserAgentHeader).toByteArray();
    if (!user_agent.isEmpty())
    {
        http_data += "User-Agent: " + user_agent + "\r\n";
    }

    // Workaround weird issue in LXD's use of Go where Go's HTTP handler thinks there is more
    // data and "sees" and "empty" query and reponds with an unexpected 400 error
    http_data += "Connection: close\r\n";

    if (!local_socket_write(http_data))
        return;

    if (op == "POST" || op == "PUT" || op == "PATCH")
    {
        http_data = "Content-Type: " + request.header(QNetworkRequest::ContentTypeHeader).toByteArray() + "\r\n";

        if (outgoingData && outgoingData->size() > 0)
        {
            auto content_length = request.header(QNetworkRequest::ContentLengthHeader).toByteArray();
            auto transfer_encoding = request.rawHeader("Transfer-Encoding").toLower();
            bool is_chunked = transfer_encoding.contains("chunked") ? true : false;

            if (!content_length.isEmpty())
            {
                if (is_chunked)
                {
                    throw mp::HttpLocalSocketException("Both the \'Content-Length\' header and \'chunked\' transfer "
                                                       "encoding cannot be set at the same time");
                }

                http_data += "Content-Length: " + content_length + "\r\n";
            }
            else if (content_length.isEmpty() && !is_chunked)
            {
                throw mp::HttpLocalSocketException(
                    "Either the \'Content-Length\' header or \'chunked\' transfer encoding must be set");
            }

            if (!transfer_encoding.isEmpty())
            {
                http_data += "Transfer-Encoding: " + transfer_encoding + "\r\n";
            }

            if (!local_socket_write(http_data + "\r\n"))
                return;

            local_socket->flush();

            outgoingData->open(QIODevice::ReadOnly);
            std::vector<char> data_buffer;
            data_buffer.reserve(max_bytes);

            int bytes_read{0};
            while ((bytes_read = outgoingData->read(data_buffer.data(), max_bytes)) > 0)
            {
                if (is_chunked && !local_socket_write(QByteArray::number(bytes_read, 16) + "\r\n"))
                    return;

                if (!local_socket_write(QByteArray::fromRawData(data_buffer.data(), bytes_read)))
                    return;

                if (is_chunked && !local_socket_write("\r\n"))
                    return;

                local_socket->waitForBytesWritten();
            }

            if (bytes_read < 0)
            {
                throw mp::HttpLocalSocketException(
                    fmt::format("Cannot read data to send to socket: {}", outgoingData->errorString()));
            }

            // Trailer part for chunked data
            if (is_chunked && !local_socket_write("0\r\n"))
                return;
        }
    }

    if (!local_socket_write("\r\n"))
        return;

    local_socket->flush();
}

void mp::LocalSocketReply::read_reply()
{
    auto data_ptr = reply_data.data() + reply_offset;
    int bytes_read{0};

    do
    {
        if (reply_offset + local_socket->bytesAvailable() > reply_data.length())
        {
            reply_data.append(len, '\0');
            data_ptr = reply_data.data() + reply_offset;
        }

        bytes_read = local_socket->read(data_ptr, len);

        reply_offset += bytes_read;
        data_ptr += bytes_read;
    } while (bytes_read > 0);
}

void mp::LocalSocketReply::read_finish()
{
    if (local_socket->bytesAvailable())
        read_reply();

    if (reply_offset)
        parse_reply();

    setFinished(true);
    emit finished();
}

void mp::LocalSocketReply::parse_reply()
{
    auto reply_lines = reply_data.split('\n');
    auto it = reply_lines.constBegin();

    parse_status(*it);

    for (++it; it != reply_lines.constEnd(); ++it)
    {
        if ((*it).contains("Transfer-Encoding") && (*it).contains("chunked"))
            chunked_transfer_encoding = true;

        if ((*it).isEmpty() || (*it).startsWith('\r'))
        {
            // Advance to the body
            // Chunked transfer encoding also includes a line with the amount of
            // bytes (in hex) in the chunk. We just skip it for now.
            it = chunked_transfer_encoding ? it + 2 : it + 1;

            content_data = (*it).trimmed();

            break;
        }
    }
}

void mp::LocalSocketReply::parse_status(const QByteArray& status)
{
    QRegularExpression http_status_regex{"^HTTP/\\d\\.\\d (?P<status>\\d{3})\\ (?P<message>.*)$"};
    QRegularExpressionMatch http_status_match = http_status_regex.match(QString::fromLatin1(status));

    if (!http_status_match.hasMatch())
    {
        setError(QNetworkReply::ProtocolFailure, "Malformed HTTP response from server");
        emit errorOccurred(QNetworkReply::ProtocolFailure);

        return;
    }

    bool ok;
    auto statusCode = http_status_match.captured("status").toInt(&ok);

    if (statusCode >= 400)
    {
        auto error_code = statusCodeFromHttp(statusCode);

        setError(error_code, http_status_match.captured("message"));
        emit errorOccurred(error_code);
    }
}

bool mp::LocalSocketReply::local_socket_write(const QByteArray& data)
{
    auto bytes_written = local_socket->write(data);
    if (bytes_written < 0)
    {
        setError(QNetworkReply::InternalServerError, local_socket->errorString());
        emit errorOccurred(QNetworkReply::InternalServerError);

        return false;
    }

    return true;
}
