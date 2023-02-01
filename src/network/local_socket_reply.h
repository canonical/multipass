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

#ifndef MULTIPASS_LOCAL_SOCKET_REPLY_H
#define MULTIPASS_LOCAL_SOCKET_REPLY_H

#include <QByteArray>
#include <QLocalSocket>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QString>

#include <memory>

namespace multipass
{
using LocalSocketUPtr = std::unique_ptr<QLocalSocket>;

class LocalSocketReply : public QNetworkReply
{
    Q_OBJECT
public:
    LocalSocketReply(LocalSocketUPtr local_socket, const QNetworkRequest& request, QIODevice* outgoingData);
    LocalSocketReply();
    virtual ~LocalSocketReply();

public Q_SLOTS:
    void abort() override;

protected:
    qint64 readData(char* data, qint64 maxSize) override;
    QByteArray content_data;

private slots:
    void read_reply();
    void read_finish();

private:
    void send_request(const QNetworkRequest& request, QIODevice* outgoingData);
    void parse_reply();
    void parse_status(const QByteArray& status);
    bool local_socket_write(const QByteArray& data);

    LocalSocketUPtr local_socket;
    QByteArray reply_data;
    qint64 reply_offset{0};
    qint64 offset{0};
    bool chunked_transfer_encoding{false};
};
} // namespace multipass

#endif // MULTIPASS_LOCAL_SOCKET_REPLY_H
