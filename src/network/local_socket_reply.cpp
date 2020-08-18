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

#include "local_socket_reply.h"

#include <multipass/exceptions/http_local_socket_exception.h>

#include <QRegularExpression>

namespace mp = multipass;

namespace
{
constexpr int len = 65536;

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
    emit error(OperationCanceledError);

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
    http_data += op;
    http_data += ' ';
    http_data += request.url().path();

    if (request.url().hasQuery())
    {
        http_data += "?";
        http_data += request.url().query();
    }

    http_data += " HTTP/1.1\r\n";

    // Build the HTTP Host header
    auto host = request.url().host();
    if (!host.isEmpty())
    {
        http_data += "Host: ";
        http_data += host;
        http_data += "\r\n";
    }
    else
    {
        throw mp::HttpLocalSocketException("Host is required in URL");
    }

    // Build the HTTP User-Agent header
    auto user_agent = request.header(QNetworkRequest::UserAgentHeader).toByteArray();
    if (!user_agent.isEmpty())
    {
        http_data += "User-Agent: ";
        http_data += user_agent;
        http_data += "\r\n";
    }

    local_socket->write(http_data);

    if (op == "POST" || op == "PUT")
    {
        http_data = "Content-Type: ";
        http_data += request.header(QNetworkRequest::ContentTypeHeader).toByteArray();
        http_data += "\r\n";

        if (outgoingData)
        {
            auto content_length = request.header(QNetworkRequest::ContentLengthHeader).toByteArray();
            auto transfer_encoding = request.rawHeader("Transfer-Encoding");

            if (!content_length.isEmpty() && !transfer_encoding.isEmpty())
            {
                throw mp::HttpLocalSocketException(
                    "Both \'Content-Length\' and \'Transfer-Encoding\' headers cannot be set at the same time");
            }
            else if (content_length.isEmpty() && transfer_encoding.isEmpty())
            {
                throw mp::HttpLocalSocketException(
                    "Either the \'Content-Length\' or the \'Transfer-Encoding\' header must be set");
            }
            else if (!content_length.isEmpty())
            {
                http_data += "Content-Length: ";
                http_data += content_length;
                http_data += "\r\n";
            }
            else
            {
                http_data += "Transfer-Encoding: ";
                http_data += transfer_encoding;
                http_data += "\r\n";
            }

            http_data += "\r\n";

            local_socket->write(http_data);
            local_socket->flush();

            outgoingData->open(QIODevice::ReadOnly);
            QByteArray data_buffer;
            const auto max_bytes{32768};

            while (true)
            {
                auto bytes_available = outgoingData->bytesAvailable();
                if (bytes_available == 0)
                    break;

                data_buffer = outgoingData->read(bytes_available);

                auto data_ptr = data_buffer.data();

                while (bytes_available > 0)
                {
                    auto bytes_to_write = bytes_available < max_bytes ? bytes_available : max_bytes;

                    if (transfer_encoding == "chunked")
                    {
                        QByteArray http_chunk_size{QByteArray::number(bytes_to_write, 16)};
                        http_chunk_size += "\r\n";
                        local_socket->write(http_chunk_size);
                    }

                    auto bytes_written = local_socket->write(data_ptr, bytes_to_write);
                    if (bytes_written < 0)
                    {
                        setError(QNetworkReply::InternalServerError, local_socket->errorString());
                        emit error(QNetworkReply::InternalServerError);

                        return;
                    }

                    if (bytes_written == 0)
                        break;

                    local_socket->write("\r\n");
                    local_socket->waitForBytesWritten();

                    bytes_available -= bytes_written;
                    data_ptr += bytes_written;
                }
            }
        }
    }

    local_socket->write("\r\n\r\n");
    local_socket->flush();
}

void mp::LocalSocketReply::read_reply()
{
    auto data_ptr = reply_data.data();
    int bytes_read{0};

    do
    {
        bytes_read = local_socket->read(reply_data.data(), len);

        data_ptr += bytes_read;
    } while (bytes_read > 0);

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

        emit error(QNetworkReply::ProtocolFailure);

        return;
    }

    bool ok;
    auto statusCode = http_status_match.captured("status").toInt(&ok);

    if (statusCode >= 400)
    {
        auto error_code = statusCodeFromHttp(statusCode);

        setError(error_code, http_status_match.captured("message"));

        emit error(error_code);
    }
}
