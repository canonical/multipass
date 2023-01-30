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

#ifndef MULTIPASS_MOCK_SERVER_WRITER_H
#define MULTIPASS_MOCK_SERVER_WRITER_H

#include <multipass/rpc/multipass.grpc.pb.h>

namespace multipass
{
namespace test
{
template <typename W, typename R>
class MockServerReaderWriter : public grpc::ServerReaderWriterInterface<W, R>
{
public:
    MOCK_METHOD(void, SendInitialMetadata, (), (override));
    MOCK_METHOD(bool, Write, (const W& msg, grpc::WriteOptions options), (override));
    MOCK_METHOD(bool, NextMessageSize, (uint32_t*), (override));
    MOCK_METHOD(bool, Read, (R*), (override));
};
} // namespace test
} // namespace multipass

#endif // MULTIPASS_MOCK_SERVER_WRITER_H
