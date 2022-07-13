/*
 * Copyright (C) 2022 Canonical, Ltd.
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
template <typename W>
class MockServerWriter : public grpc::ServerWriterInterface<W>
{
public:
    MOCK_METHOD0(SendInitialMetadata, void());
    MOCK_METHOD2_T(Write, bool(const W& msg, grpc::WriteOptions options));
};
} // namespace test
} // namespace multipass

#endif // MULTIPASS_MOCK_SERVER_WRITER_H
