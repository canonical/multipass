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

#ifndef MULTIPASS_LOCAL_SOCKET_CONNECTION_EXCEPTION_H
#define MULTIPASS_LOCAL_SOCKET_CONNECTION_EXCEPTION_H

#include <QLocalSocket>

#include <stdexcept>

namespace multipass
{
class LocalSocketConnectionException : public std::runtime_error
{
public:
    LocalSocketConnectionException(const std::string& what, const QLocalSocket::LocalSocketError& error_code)
        : runtime_error(what), error_code{error_code}
    {
    }

    QLocalSocket::LocalSocketError get_error() const
    {
        return error_code;
    }

private:
    const QLocalSocket::LocalSocketError error_code;
};
} // namespace multipass
#endif // MULTIPASS_HTTP_LOCAL_SOCKET_EXCEPTION_H
