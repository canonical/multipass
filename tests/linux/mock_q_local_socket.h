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

#ifndef MULTIPASS_MOCK_Q_LOCAL_SOCKET_H
#define MULTIPASS_MOCK_Q_LOCAL_SOCKET_H

#include <QLocalSocket>

namespace multipass
{
namespace test
{
class MockQLocalSocket : public QLocalSocket
{
public:
    MockQLocalSocket(int writes_before_failure) : writes_before_failure{writes_before_failure}
    {
        QIODevice::open(QIODevice::ReadWrite);
    };

    virtual qint64 writeData(const char* data, qint64 c) override
    {
        ++num_writes;

        if (writes_before_failure-- > 0)
            return c;
        else
            return -1;
    };

    virtual bool waitForBytesWritten(int msecs = 30000) override
    {
        return true;
    };

    int num_writes_called()
    {
        return num_writes;
    };

private:
    int writes_before_failure;
    int num_writes{0};
};
} // namespace test
} // namespace multipass

#endif // MULTIPASS_MOCK_Q_LOCAL_SOCKET_H
