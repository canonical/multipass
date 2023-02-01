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

#include "local_socket_server_test_fixture.h"
#include "mock_q_buffer.h"
#include "mock_q_local_socket.h"

#include "tests/common.h"
#include "tests/temp_dir.h"

#include <src/network/local_socket_reply.h>

#include <multipass/exceptions/http_local_socket_exception.h>
#include <multipass/exceptions/local_socket_connection_exception.h>
#include <multipass/network_access_manager.h>

#include <QBuffer>
#include <QEventLoop>
#include <QNetworkReply>
#include <QTimer>

#include <random>

namespace mp = multipass;
namespace mpt = multipass::test;

using namespace testing;

namespace
{
using HTTPErrorParamType = std::pair<QByteArray, QNetworkReply::NetworkError>;
constexpr auto max_bytes{32768};
constexpr auto max_content{65536};

auto generate_random_data(int length)
{
    const std::string str = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    std::random_device rd;
    std::mt19937 generator(rd());
    std::uniform_int_distribution<int> dist(0, str.size() - 1);
    QByteArray random_data;

    for (int i = 0; i < length; ++i)
    {
        random_data += str[dist(generator)];
    }

    return random_data;
}

struct LocalNetworkAccessManager : public Test
{
    LocalNetworkAccessManager()
        : socket_path{QString("%1/test_socket").arg(temp_dir.path())},
          test_server{mpt::MockLocalSocketServer(socket_path)},
          base_url{QString("unix://%1@1.0").arg(socket_path)}
    {
        download_timeout.setInterval(2000);
        base_url.setHost("test");
    }

    auto handle_request(const QUrl& url, const QByteArray& verb, const QByteArray& data = QByteArray())
    {
        QNetworkRequest request{url};
        request.setHeader(QNetworkRequest::UserAgentHeader, "Test");

        if (!data.isEmpty())
        {
            request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

            auto data_size = data.size();
            if (data_size < max_bytes)
            {
                request.setHeader(QNetworkRequest::ContentLengthHeader, data.size());
            }
            else
            {
                request.setRawHeader("Transfer-Encoding", "chunked");
            }
        }

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
    mpt::TempDir temp_dir;
    QString socket_path;
    mpt::MockLocalSocketServer test_server;
    QUrl base_url;
    QEventLoop event_loop;
    QTimer download_timeout;
};

struct HTTPErrorsTestSuite : LocalNetworkAccessManager, WithParamInterface<HTTPErrorParamType>
{
};

struct LocalSocketWriteErrorTestSuite : LocalNetworkAccessManager, WithParamInterface<int>
{
};

const std::vector<int> local_socket_write_error_suite_inputs{0, 1, 2, 3, 4, 5, 6};

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
    http_response += "\r\n";

    auto server_response = [&http_response](auto...) { return http_response; };
    test_server.local_socket_server_handler(server_response);

    auto reply = handle_request(base_url, "GET");

    EXPECT_EQ(reply->error(), QNetworkReply::NoError);
}

TEST_F(LocalNetworkAccessManager, reads_expected_data_not_chunked)
{
    QByteArray reply_data{"Hello"};

    QByteArray http_response;
    http_response += "HTTP/1.1 200 OK\r\n";
    http_response += "Content-Length: 5\r\n";
    http_response += "\r\n";
    http_response += reply_data;
    http_response += "\r\n";

    auto server_response = [&http_response](auto...) { return http_response; };
    test_server.local_socket_server_handler(server_response);

    auto reply = handle_request(base_url, "GET");

    ASSERT_EQ(reply->error(), QNetworkReply::NoError);

    auto data = reply->readAll();

    EXPECT_EQ(data, reply_data);
}

TEST_F(LocalNetworkAccessManager, reads_expected_data_chunked)
{
    QByteArray reply_data{"What's up?"};

    QByteArray http_response;
    http_response += "HTTP/1.1 200 OK\r\n";
    http_response += "Content-Length: 10\r\n";
    http_response += "Transfer-Encoding: chunked\r\n";
    http_response += "\r\n";
    http_response += "a\r\n";
    http_response += reply_data;
    http_response += "\r\n";

    auto server_response = [&http_response](auto...) { return http_response; };
    test_server.local_socket_server_handler(server_response);

    auto reply = handle_request(base_url, "GET");

    ASSERT_EQ(reply->error(), QNetworkReply::NoError);

    auto data = reply->readAll();

    EXPECT_EQ(data, reply_data);
}

TEST_F(LocalNetworkAccessManager, client_posts_correct_data)
{
    QByteArray expected_data{"POST /1.0 HTTP/1.1\r\n"
                             "Host: test\r\n"
                             "User-Agent: Test\r\n"
                             "Connection: close\r\n"
                             "Content-Type: application/x-www-form-urlencoded\r\n"
                             "Content-Length: 11\r\n\r\n"
                             "Hello World\r\n"};

    QByteArray http_response{"HTTP/1.1 200 OK\r\n\r\n"};

    auto server_response = [&http_response, &expected_data](auto data) {
        EXPECT_EQ(data, expected_data);
        return http_response;
    };

    test_server.local_socket_server_handler(server_response);

    handle_request(base_url, "POST", "Hello World");
}

TEST_F(LocalNetworkAccessManager, bad_http_server_response_has_error)
{
    QByteArray malformed_http_response{"FOO/1.4 42 Yo\r\n"};

    auto server_response = [&malformed_http_response](auto...) { return malformed_http_response; };
    test_server.local_socket_server_handler(server_response);

    auto reply = handle_request(base_url, "GET");

    EXPECT_EQ(reply->error(), QNetworkReply::ProtocolFailure);
}

TEST_F(LocalNetworkAccessManager, malformed_unix_schema_throws)
{
    base_url = "unix:///foo";

    QNetworkRequest request{base_url};

    EXPECT_THROW(manager.sendCustomRequest(request, "GET"), mp::LocalSocketConnectionException);
}

TEST_F(LocalNetworkAccessManager, unable_to_connect_throws)
{
    base_url = "unix:///invalid/path@1.0";

    QNetworkRequest request{base_url};

    EXPECT_THROW(manager.sendCustomRequest(request, "GET"), mp::LocalSocketConnectionException);
}

TEST_F(LocalNetworkAccessManager, reply_abort_sets_expected_error)
{
    download_timeout.setInterval(2);

    auto reply = handle_request(base_url, "GET");

    EXPECT_EQ(reply->error(), QNetworkReply::OperationCanceledError);
}

TEST_F(LocalNetworkAccessManager, other_request_uses_qnam)
{
    QUrl url{QString("file://%1/missing_doc.txt").arg(temp_dir.path())};

    auto reply = handle_request(url, "GET");

    EXPECT_EQ(reply->error(), QNetworkReply::ProtocolUnknownError);
}

TEST_F(LocalNetworkAccessManager, query_in_url_is_preserved)
{
    const QString query_string{"query=foo"};

    QByteArray http_response;
    http_response += "HTTP/1.1 200 OK\r\n";
    http_response += "\r\n";

    base_url.setQuery(query_string);

    auto server_response = [&http_response, &query_string](auto data) {
        EXPECT_TRUE(data.contains(query_string.toLatin1()));
        return http_response;
    };

    test_server.local_socket_server_handler(server_response);

    handle_request(base_url, "GET");
}

TEST_F(LocalNetworkAccessManager, sending_chunked_data_receives_expected_data)
{
    QByteArray random_data = generate_random_data(max_content);
    QByteArray http_response{"HTTP/1.1 200 OK\r\n\r\n"};

    auto server_response = [&http_response, &random_data](auto data) {
        QByteArray first_part = random_data.mid(0, 32768);
        QByteArray second_part = random_data.mid(32768);

        // Implies the data was split, ie, chunked
        EXPECT_FALSE(data.contains(random_data));

        // Implies that the correct data was still received
        EXPECT_TRUE(data.contains(first_part));
        EXPECT_TRUE(data.contains(second_part));

        return http_response;
    };

    test_server.local_socket_server_handler(server_response);

    handle_request(base_url, "POST", random_data);
}

TEST_F(LocalNetworkAccessManager, overflowing_response_works)
{
    auto reply_data = generate_random_data(max_content * 2);
    QByteArray http_response;
    http_response += "HTTP/1.1 200 OK\r\n";
    http_response += "Content-Length: 131072\r\n";
    http_response += "\r\n";
    http_response += reply_data;
    http_response += "\r\n";

    auto server_response = [&http_response](auto...) { return http_response; };
    test_server.local_socket_server_handler(server_response);

    auto reply = handle_request(base_url, "GET");

    ASSERT_EQ(reply->error(), QNetworkReply::NoError);

    auto data = reply->readAll();

    EXPECT_EQ(data, reply_data);
}

TEST_F(LocalNetworkAccessManager, no_host_set_throws)
{
    base_url.setHost("");

    QNetworkRequest request{base_url};

    EXPECT_THROW(manager.sendCustomRequest(request, "GET"), mp::HttpLocalSocketException);
}

TEST_F(LocalNetworkAccessManager, content_length_and_transfer_encoding_both_set_throws)
{
    QNetworkRequest request{base_url};
    QByteArray some_data{"This is some data"};

    request.setHeader(QNetworkRequest::ContentLengthHeader, some_data.size());
    request.setRawHeader("Transfer-Encoding", "chunked");

    EXPECT_THROW(manager.sendCustomRequest(request, "POST", some_data), mp::HttpLocalSocketException);
}

TEST_F(LocalNetworkAccessManager, content_length_and_transfer_encoding_not_set_throws)
{
    QNetworkRequest request{base_url};
    QByteArray some_data{"This is some data"};

    EXPECT_THROW(manager.sendCustomRequest(request, "POST", some_data), mp::HttpLocalSocketException);
}

TEST_F(LocalNetworkAccessManager, qiodevice_read_fails_throws)
{
    auto mock_q_local_socket = std::make_unique<mpt::MockQLocalSocket>(10); // Not failing any writes

    base_url.setHost("test");
    QNetworkRequest request{base_url};

    request.setAttribute(QNetworkRequest::CustomVerbAttribute, "POST");
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
    request.setRawHeader("Transfer-Encoding", "chunked");

    auto data{generate_random_data(32768)};
    mpt::MockQBuffer buffer{&data};

    EXPECT_THROW(mp::LocalSocketReply local_socket_reply(std::move(mock_q_local_socket), request, &buffer),
                 mp::HttpLocalSocketException);
    EXPECT_TRUE(buffer.read_attempted());
}

TEST_P(HTTPErrorsTestSuite, returns_expected_error)
{
    const auto http_response = GetParam().first;
    const auto expected_error = GetParam().second;

    auto server_response = [&http_response](auto...) { return http_response; };
    test_server.local_socket_server_handler(server_response);

    auto reply = handle_request(base_url, "GET");

    EXPECT_EQ(reply->error(), expected_error);
}

TEST_P(LocalSocketWriteErrorTestSuite, write_fails_emits_error_and_returns)
{
    const int writes_before_failure = GetParam();
    auto mock_q_local_socket = std::make_unique<mpt::MockQLocalSocket>(writes_before_failure);
    auto mock_q_local_socket_ptr = mock_q_local_socket.get();

    base_url.setHost("test");
    QNetworkRequest request{base_url};

    request.setAttribute(QNetworkRequest::CustomVerbAttribute, "POST");
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
    request.setRawHeader("Transfer-Encoding", "chunked");

    auto data{generate_random_data(32768)};
    QBuffer buffer{&data};

    mp::LocalSocketReply local_socket_reply{std::move(mock_q_local_socket), request, &buffer};

    EXPECT_EQ(local_socket_reply.error(), QNetworkReply::InternalServerError);
    EXPECT_EQ(mock_q_local_socket_ptr->num_writes_called(), writes_before_failure + 1);
}

INSTANTIATE_TEST_SUITE_P(LocalNetworkAccessManager, HTTPErrorsTestSuite, ValuesIn(http_error_suite_inputs));

INSTANTIATE_TEST_SUITE_P(LocalNetworkAccessManager, LocalSocketWriteErrorTestSuite,
                         ValuesIn(local_socket_write_error_suite_inputs));
