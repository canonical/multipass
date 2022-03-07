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
class RemoteSettingsTest : public Test
{
public:
    void SetUp() override
    {
        EXPECT_CALL(mock_term, cout).WillRepeatedly(ReturnRef(fake_cout));
        EXPECT_CALL(mock_term, cerr).WillRepeatedly(ReturnRef(fake_cerr));
    }

public:
    std::ostringstream fake_cout;
    std::ostringstream fake_cerr;
    mpt::MockTerminal mock_term;
    StrictMock<mpt::MockRpcStub> mock_stub{};
};

TEST_F(RemoteSettingsTest, savesProvidedKeyPrefix)
{
    constexpr auto prefix = "my.prefix";
    mp::RemoteSettingsHandler handler{prefix, mock_stub, &mock_term, 1};

    EXPECT_EQ(handler.get_key_prefix(), prefix);
}

TEST_F(RemoteSettingsTest, savesProvidedVerbosity)
{
    constexpr auto verbosity = 42;
    mp::RemoteSettingsHandler handler{"prefix", mock_stub, &mock_term, verbosity};

    EXPECT_EQ(handler.get_verbosity(), verbosity);
}

TEST_F(RemoteSettingsTest, honorsVerbosityInKeysRequest)
{
    constexpr auto verbosity = 5;
    EXPECT_CALL(mock_stub, keysRaw(_, Property(&mp::KeysRequest::verbosity_level, Eq(verbosity))))
        .WillOnce(ReturnNew<mpt::MockClientReader<mp::KeysReply>>());

    mp::RemoteSettingsHandler handler{"prefix", mock_stub, &mock_term, verbosity};
    handler.keys();
}

TEST_F(RemoteSettingsTest, honorsVerbosityInGetRequest)
{
    constexpr auto verbosity = 6;
    EXPECT_CALL(mock_stub, getRaw(_, Property(&mp::GetRequest::verbosity_level, Eq(verbosity))))
        .WillOnce(ReturnNew<mpt::MockClientReader<mp::GetReply>>());

    mp::RemoteSettingsHandler handler{"", mock_stub, &mock_term, verbosity};
    handler.get("whatever");
}

TEST_F(RemoteSettingsTest, honorsVerbosityInSetRequest)
{
    constexpr auto verbosity = 7;
    EXPECT_CALL(mock_stub, setRaw(_, Property(&mp::SetRequest::verbosity_level, Eq(verbosity))))
        .WillOnce(ReturnNew<mpt::MockClientReader<mp::SetReply>>());

    mp::RemoteSettingsHandler handler{"", mock_stub, &mock_term, verbosity};
    handler.set("whatever", "works");
}

TEST_F(RemoteSettingsTest, keysEmptyByDefault)
{
    EXPECT_CALL(mock_stub, keysRaw).WillOnce(ReturnNew<mpt::MockClientReader<mp::KeysReply>>());
    mp::RemoteSettingsHandler handler{"prefix", mock_stub, &mock_term, 31};

    EXPECT_THAT(handler.keys(), IsEmpty());
}

TEST_F(RemoteSettingsTest, returnsRemoteKeys)
{
    constexpr auto some_keys = std::array{"a.b", "c.d.e", "f"};
    auto mock_client_reader = std::make_unique<mpt::MockClientReader<mp::KeysReply>>(); /* use unique_ptr to
         avoid leaking on any exception (hopefully none, but just to be sure) until we transfer ownership */

    EXPECT_CALL(*mock_client_reader, Read).WillOnce([&some_keys](mp::KeysReply* reply) {
        reply->mutable_settings_keys()->Add(some_keys.begin(), some_keys.end());
        return false;
    });

    EXPECT_CALL(mock_stub, keysRaw).WillOnce([&mock_client_reader] { return mock_client_reader.release(); }); /*
    transfer ownership - we can't just `Return(mock_client_reader.release())` because that would release the ptr right
    away, to be adopted only if and when the mock was called */

    mp::RemoteSettingsHandler handler{"prefix", mock_stub, &mock_term, 99};
    EXPECT_THAT(handler.keys(), UnorderedElementsAreArray(some_keys));
}

TEST_F(RemoteSettingsTest, returnsPlaceholderKeysWhenDaemonNotFound)
{
    auto mock_client_reader = std::make_unique<mpt::MockClientReader<mp::KeysReply>>(); // idem
    EXPECT_CALL(*mock_client_reader, Finish)
        .WillOnce(Return(grpc::Status{grpc::StatusCode::NOT_FOUND, "remote not around"}));

    EXPECT_CALL(mock_stub, keysRaw).WillOnce([&mock_client_reader] { return mock_client_reader.release(); }); // idem

    auto prefix = "remote.";
    mp::RemoteSettingsHandler handler{prefix, mock_stub, &mock_term, 0};

    EXPECT_THAT(handler.keys(), ElementsAre(mpt::match_qstring(StartsWith(std::string{prefix} + "*"))));
}

TEST_F(RemoteSettingsTest, throwsOnOtherErrorContactingRemote)
{
    constexpr auto code = grpc::StatusCode::INTERNAL;
    auto mock_client_reader = std::make_unique<mpt::MockClientReader<mp::KeysReply>>(); // idem
    EXPECT_CALL(*mock_client_reader, Finish).WillOnce(Return(grpc::Status{code, "unexpected"}));

    EXPECT_CALL(mock_stub, keysRaw).WillOnce([&mock_client_reader] { return mock_client_reader.release(); }); // idem

    mp::RemoteSettingsHandler handler{"prefix.", mock_stub, &mock_term, 789};
    MP_EXPECT_THROW_THAT(
        handler.keys(), mp::RemoteHandlerException,
        Property(&mp::RemoteHandlerException::get_status, Property(&grpc::Status::error_code, Eq(code))));
}
} // namespace
