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

#ifndef MULTIPASS_MOCK_LOCAL_SOCKET_REPLY_H
#define MULTIPASS_MOCK_LOCAL_SOCKET_REPLY_H

#include <src/network/local_socket_reply.h>

namespace multipass
{
namespace test
{
struct MockLocalSocketReply : public LocalSocketReply
{
    MockLocalSocketReply(const QByteArray& data, const QNetworkReply::NetworkError error = QNetworkReply::NoError)
    {
        content_data = data;
        setError(error, "Error");
    };

    // Needed since setFinished is protected in QNetworkReply
    void setFinished(bool finished)
    {
        QNetworkReply::setFinished(finished);
    };
};
} // namespace test
} // namespace multipass

#endif // MULTIPASS_MOCK_LOCAL_SOCKET_REPLY_H
