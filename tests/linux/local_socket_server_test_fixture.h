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

#ifndef MULTIPASS_LOCAL_SOCKET_SERVER_TEST_FIXTURE_H
#define MULTIPASS_LOCAL_SOCKET_SERVER_TEST_FIXTURE_H

#include <QByteArray>
#include <QLocalServer>
#include <QLocalSocket>
#include <QString>

namespace multipass
{
namespace test
{
class MockLocalSocketServer
{
public:
    MockLocalSocketServer(const QString& socket_path)
    {
        test_server.listen(socket_path);
    }

    template <typename Handler>
    void local_socket_server_handler(Handler&& response_handler)
    {
        QObject::connect(&test_server, &QLocalServer::newConnection, [&] {
            auto client_connection = test_server.nextPendingConnection();

            client_connection->waitForReadyRead();
            auto data = client_connection->readAll();

            auto response = response_handler(data);

            // Simulate data arriving in chunks
            int left = response.length() / 2;
            client_connection->write(response.left(left));
            client_connection->flush();
            client_connection->write(response.right(response.length() - left));
            client_connection->close();
        });
    }

private:
    QLocalServer test_server;
};
} // namespace test
} // namespace multipass

#endif /* MULTIPASS_LOCAL_SOCKET_SERVER_TEST_FIXTURE_H */
