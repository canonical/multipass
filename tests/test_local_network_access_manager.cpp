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

#include "temp_dir.h"

#include <multipass/network_access_manager.h>

#include <QEventLoop>
#include <QLocalServer>
#include <QLocalSocket>
#include <QNetworkReply>
#include <QTimer>

#include <gmock/gmock.h>

namespace mp = multipass;
namespace mpt = multipass::test;

using namespace testing;

namespace
{
using HTTPErrorParamType = std::pair<QByteArray, QNetworkReply::NetworkError>;

struct LocalNetworkAccessManager : public Test
{
    LocalNetworkAccessManager()
    {
        socket_path = QString("%1/test_socket").arg(temp_dir.path());
        test_server.listen(socket_path);
        base_url = QString("unix://%1@1.0").arg(socket_path);
    }

    void local_socket_server_handler(const QByteArray& http_response, const QByteArray& expected_data = QByteArray())
    {
        QObject::connect(&test_server, &QLocalServer::newConnection, [&] {
            auto client_connection = test_server.nextPendingConnection();

            client_connection->waitForReadyRead();
            auto data = client_connection->readAll();

            if (!expected_data.isEmpty())
            {
                EXPECT_THAT(data, Eq(expected_data));
            }

            client_connection->write(http_response);
        });
    }

    auto handle_request(const QUrl& url, const QByteArray& verb, const QByteArray& data = QByteArray())
    {
        QNetworkRequest request{url};

        std::unique_ptr<QNetworkReply> reply{manager.sendCustomRequest(request, verb, data)};

        QObject::connect(reply.get(), &QNetworkReply::finished, &event_loop, &QEventLoop::quit);
        QObject::connect(&download_timeout, &QTimer::timeout, [&] {
            download_timeout.stop();
            reply->abort();
        });

        download_timeout.start();
        event_loop.exec();

        return reply;
    }

    mp::NetworkAccessManager manager;
    QLocalServer test_server;
    mpt::TempDir temp_dir;
    QString socket_path;
    QUrl base_url;
    QEventLoop event_loop;
    QTimer download_timeout;
};

struct HTTPErrorsTestSuite : LocalNetworkAccessManager, WithParamInterface<HTTPErrorParamType>
{
};

const std::vector<HTTPErrorParamType> http_error_suite_inputs{
    {"HTTP/1.1 400 Bad Request\r\n\r\n", QNetworkReply::ProtocolInvalidOperationError},
    {"HTTP/1.1 401 Authorization Required\r\n\r\n", QNetworkReply::AuthenticationRequiredError},
    {"HTTP/1.1 403 Access Denied\r\n\r\n", QNetworkReply::ContentAccessDenied},
    {"HTTP/1.1 404 Not Found\r\n\r\n", QNetworkReply::ContentNotFoundError},
    {"HTTP/1.1 409 Resource Conflict\r\n\r\n", QNetworkReply::ContentConflictError},
    {"HTTP/1.1 500 Internal Server Error\r\n\r\n", QNetworkReply::InternalServerError},
    {"HTTP/1.1 501 Unknown Server Error\r\n\r\n", QNetworkReply::UnknownServerError},
    {"HTTP/1.1 412 Precondition Failed\r\n\r\n", QNetworkReply::UnknownContentError}};
} // namespace

TEST_F(LocalNetworkAccessManager, no_error_returns_good_reply)
{
    QByteArray http_response;
    http_response += "HTTP/1.1 200 OK\r\n";
    http_response += "Content-Type: text/plain\r\n";
    http_response += "\r\n";

    local_socket_server_handler(http_response);

    auto reply = handle_request(base_url, "GET");

    EXPECT_THAT(reply->error(), Eq(QNetworkReply::NoError));
}

TEST_F(LocalNetworkAccessManager, reads_expected_data_not_chunked)
{
    QByteArray reply_data{"Hello"};

    QByteArray http_response;
    http_response += "HTTP/1.1 200 OK\r\n";
    http_response += "Content-Type: text/plain\r\n";
    http_response += "Content-Length: 5\r\n";
    http_response += "\r\n";
    http_response += reply_data;
    http_response += "\r\n";

    local_socket_server_handler(http_response);

    auto reply = handle_request(base_url, "GET");

    ASSERT_THAT(reply->error(), Eq(QNetworkReply::NoError));

    auto data = reply->readAll();

    EXPECT_THAT(data, Eq(reply_data));
}

TEST_F(LocalNetworkAccessManager, reads_expected_data_chunked)
{
    QByteArray reply_data{"What's up?"};

    QByteArray http_response;
    http_response += "HTTP/1.1 200 OK\r\n";
    http_response += "Content-Type: text/plain\r\n";
    http_response += "Content-Length: 5\r\n";
    http_response += "Transfer-Encoding: chunked\r\n";
    http_response += "\r\n";
    http_response += "5\r\n";
    http_response += reply_data;
    http_response += "\r\n";

    local_socket_server_handler(http_response);

    auto reply = handle_request(base_url, "GET");

    ASSERT_THAT(reply->error(), Eq(QNetworkReply::NoError));

    auto data = reply->readAll();

    EXPECT_THAT(data, Eq(reply_data));
}

TEST_F(LocalNetworkAccessManager, client_posts_correct_data)
{
    QByteArray expected_data{"POST /1.0 HTTP/1.1\r\n"
                             "Host: multipass\r\n"
                             "User-Agent: Mozilla/5.0\r\n"
                             "Accept: */*\r\n"
                             "Content-Type: application/x-www-form-urlencoded\r\n"
                             "Content-Length: 11\r\n\r\n"
                             "Hello World\r\n"};

    QByteArray http_response;
    http_response += "HTTP/1.1 200 OK\r\n";
    http_response += "Content-Type: text/plain\r\n";
    http_response += "\r\n";

    // The actual test is in the QObject::connect's lambda slot
    local_socket_server_handler(http_response, expected_data);

    handle_request(base_url, "POST", "Hello World");
}

TEST_F(LocalNetworkAccessManager, bad_http_server_response_has_error)
{
    QByteArray malformed_http_response{"FOO/1.4 42 Yo\r\n"};

    local_socket_server_handler(malformed_http_response);

    auto reply = handle_request(base_url, "GET");

    EXPECT_THAT(reply->error(), Eq(QNetworkReply::ProtocolFailure));
}

TEST_F(LocalNetworkAccessManager, malformed_unix_schema_throws)
{
    base_url = "unix:///foo";

    QNetworkRequest request{base_url};

    EXPECT_THROW(manager.sendCustomRequest(request, "GET"), std::runtime_error);
}

TEST_F(LocalNetworkAccessManager, unable_to_connect_throws)
{
    base_url = "unix:///invalid/path@1.0";

    QNetworkRequest request{base_url};

    EXPECT_THROW(manager.sendCustomRequest(request, "GET"), std::runtime_error);
}

TEST_P(HTTPErrorsTestSuite, returns_expected_error)
{
    const auto http_response = GetParam().first;
    const auto expected_error = GetParam().second;

    local_socket_server_handler(http_response);

    auto reply = handle_request(base_url, "GET");

    EXPECT_THAT(reply->error(), Eq(expected_error));
}

INSTANTIATE_TEST_SUITE_P(LocalNetworkAccessManager, HTTPErrorsTestSuite, ValuesIn(http_error_suite_inputs));
