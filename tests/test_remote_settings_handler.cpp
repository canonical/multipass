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

    template <typename ReplyType>
    static std::unique_ptr<mpt::MockClientReader<ReplyType>> make_mock_reader() // just a helper to minimize boilerplate
    {
        return std::make_unique<mpt::MockClientReader<ReplyType>>();
    }

    // Create a callable that returns an owning plain pointer (as expected by grpc), but spares ownership until called.
    // This is useful in mock actions for grpc functions that return ownership-transferring ptrs.
    // Note that we'd better not just `EXPECT_CALL(...).Return(uptr.release())` because that would release the ptr right
    // away, to be adopted only if and when the mock was called.
    template <typename T>
    static auto make_releaser(std::unique_ptr<T>& uptr) // uptr avoids leaking on exceptions until ownership transfer
    {
        return [&uptr] // we capture by reference - no leaks if uptr is destroyed before lambda called
        {
            return uptr.release(); /* transfer ownership */
        };
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
    auto mock_client_reader = make_mock_reader<mp::KeysReply>();

    EXPECT_CALL(*mock_client_reader, Read).WillOnce([&some_keys](mp::KeysReply* reply) {
        reply->mutable_settings_keys()->Add(some_keys.begin(), some_keys.end());
        return false;
    });

    EXPECT_CALL(mock_stub, keysRaw).WillOnce(make_releaser(mock_client_reader)); /* grpc code claims ownership of the
                                                                                    ptr that keysRaw() returns */

    mp::RemoteSettingsHandler handler{"prefix", mock_stub, &mock_term, 99};
    EXPECT_THAT(handler.keys(), UnorderedElementsAreArray(some_keys));
}

TEST_F(RemoteSettingsTest, returnsPlaceholderKeysWhenDaemonNotFound)
{
    auto mock_client_reader = make_mock_reader<mp::KeysReply>();
    EXPECT_CALL(*mock_client_reader, Finish)
        .WillOnce(Return(grpc::Status{grpc::StatusCode::NOT_FOUND, "remote not around"}));

    EXPECT_CALL(mock_stub, keysRaw).WillOnce(make_releaser(mock_client_reader));

    auto prefix = "remote.";
    mp::RemoteSettingsHandler handler{prefix, mock_stub, &mock_term, 0};

    EXPECT_THAT(handler.keys(), ElementsAre(mpt::match_qstring(StartsWith(std::string{prefix} + "*"))));
}

TEST_F(RemoteSettingsTest, throwsOnOtherErrorContactingRemote)
{
    constexpr auto error_code = grpc::StatusCode::INTERNAL;
    constexpr auto error_msg = "unexpected";
    constexpr auto error_details = "details";

    auto mock_client_reader = make_mock_reader<mp::KeysReply>();
    EXPECT_CALL(*mock_client_reader, Finish).WillOnce(Return(grpc::Status{error_code, error_msg, error_details}));

    EXPECT_CALL(mock_stub, keysRaw).WillOnce(make_releaser(mock_client_reader));

    mp::RemoteSettingsHandler handler{"prefix.", mock_stub, &mock_term, 789};
    MP_EXPECT_THROW_THAT(handler.keys(), mp::RemoteHandlerException,
                         Property(&mp::RemoteHandlerException::get_status,
                                  AllOf(Property(&grpc::Status::error_code, Eq(error_code)),
                                        Property(&grpc::Status::error_message, Eq(error_msg)),
                                        Property(&grpc::Status::error_details, Eq(error_details)))));
}
} // namespace
