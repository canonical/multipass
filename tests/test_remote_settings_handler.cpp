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

#include "common.h"
#include "mock_client_rpc.h"
#include "mock_terminal.h"

#include <src/client/cli/cmd/remote_settings_handler.h>

#include <sstream>

namespace mp = multipass;
namespace mpt = mp::test;
using namespace testing;

namespace
{
TEST(RemoteSettingsTest, savesProvidedKeyPrefix)
{
    constexpr auto prefix = "my.prefix";
    auto mock_stub = StrictMock<mpt::MockRpcStub>{};
    auto mock_term = mpt::MockTerminal{};

    mp::RemoteSettingsHandler handler{prefix, mock_stub, &mock_term, 1};
    EXPECT_EQ(handler.get_key_prefix(), prefix);
}

TEST(RemoteSettingsTest, savesProvidedVerbosity)
{
    constexpr auto verbosity = 42;
    auto mock_stub = StrictMock<mpt::MockRpcStub>{};
    auto mock_term = mpt::MockTerminal{};

    mp::RemoteSettingsHandler handler{"prefix", mock_stub, &mock_term, verbosity};
    EXPECT_EQ(handler.get_verbosity(), verbosity);
}

TEST(RemoteSettingsTest, keysEmptyByDefault)
{
    auto fake_cout = std::ostringstream{};
    auto fake_cerr = std::ostringstream{};
    auto mock_term = mpt::MockTerminal{};
    EXPECT_CALL(mock_term, cout).WillOnce(ReturnRef(fake_cout));
    EXPECT_CALL(mock_term, cerr).WillOnce(ReturnRef(fake_cerr));

    auto mock_client_reader = std::make_unique<StrictMock<mpt::MockClientReader<mp::KeysReply>>>(); /* use unique_ptr to
    avoid leaking on any exception until we transfer ownership (hopefully none, but just to be sure) */
    EXPECT_CALL(*mock_client_reader, Read).WillOnce(Return(false));
    EXPECT_CALL(*mock_client_reader, Finish).WillOnce(Return(grpc::Status::OK));

    auto mock_stub = StrictMock<mpt::MockRpcStub>{};
    EXPECT_CALL(mock_stub, keysRaw).WillOnce([&mock_client_reader] { return mock_client_reader.release(); }); /*
    transfer ownership - we can't just `Return(mock_client_reader.release())` because that would release the ptr right
    away, to be adopted only if and when the mock was called */

    mp::RemoteSettingsHandler handler{"prefix", mock_stub, &mock_term, 31};
    EXPECT_THAT(handler.keys(), IsEmpty());
}
} // namespace
