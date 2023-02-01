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

#ifndef MULTIPASS_MOCK_Q_BUFFER_H
#define MULTIPASS_MOCK_Q_BUFFER_H

#include <QBuffer>

namespace multipass
{
namespace test
{
class MockQBuffer : public QBuffer
{
public:
    MockQBuffer(QByteArray* byteArray) : QBuffer(byteArray){};

    virtual qint64 readData(char* data, qint64 len) override
    {
        read_called = true;

        return -1;
    };

    bool read_attempted()
    {
        return read_called;
    }

private:
    bool read_called{false};
};
} // namespace test
} // namespace multipass

#endif // MULTIPASS_MOCK_Q_BUFFER_H
