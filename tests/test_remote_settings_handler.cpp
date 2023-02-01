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

#include "common.h"
#include "mock_client_rpc.h"
#include "mock_terminal.h"

#include <src/client/cli/cmd/remote_settings_handler.h>

#include <multipass/exceptions/settings_exceptions.h>

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

    template <typename RequestType, typename ReplyType>
    static std::unique_ptr<mpt::MockClientReaderWriter<RequestType, ReplyType>>
    make_mock_reader_writer() // just a helper to minimize boilerplate
    {
        return std::make_unique<mpt::MockClientReaderWriter<RequestType, ReplyType>>();
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

    static auto make_status_matcher(grpc::StatusCode code, const std::string& msg, const std::string& details)
    {
        return AllOf(Property(&grpc::Status::error_code, Eq(code)), Property(&grpc::Status::error_message, Eq(msg)),
                     Property(&grpc::Status::error_details, Eq(details)));
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

    auto mock_client = make_mock_reader_writer<mp::KeysRequest, mp::KeysReply>();

    EXPECT_CALL(*mock_client, Write(Property(&mp::KeysRequest::verbosity_level, Eq(verbosity)), _))
        .WillOnce(Return(true));
    EXPECT_CALL(mock_stub, keysRaw).WillOnce(make_releaser(mock_client));

    mp::RemoteSettingsHandler handler{"prefix", mock_stub, &mock_term, verbosity};
    handler.keys();
}

TEST_F(RemoteSettingsTest, honorsVerbosityInGetRequest)
{
    constexpr auto verbosity = 6;

    auto mock_client = make_mock_reader_writer<mp::GetRequest, mp::GetReply>();

    EXPECT_CALL(*mock_client, Write(Property(&mp::GetRequest::verbosity_level, Eq(verbosity)), _))
        .WillOnce(Return(true));
    EXPECT_CALL(mock_stub, getRaw).WillOnce(make_releaser(mock_client));

    mp::RemoteSettingsHandler handler{"", mock_stub, &mock_term, verbosity};
    handler.get("whatever");
}

TEST_F(RemoteSettingsTest, honorsVerbosityInSetRequest)
{
    constexpr auto verbosity = 7;

    auto mock_client = make_mock_reader_writer<mp::SetRequest, mp::SetReply>();

    EXPECT_CALL(*mock_client, Write(Property(&mp::SetRequest::verbosity_level, Eq(verbosity)), _))
        .WillOnce(Return(true));
    EXPECT_CALL(mock_stub, setRaw).WillOnce(make_releaser(mock_client));

    mp::RemoteSettingsHandler handler{"", mock_stub, &mock_term, verbosity};
    handler.set("whatever", "works");
}

TEST_F(RemoteSettingsTest, keysEmptyByDefault)
{
    auto mock_client = make_mock_reader_writer<mp::KeysRequest, mp::KeysReply>();

    EXPECT_CALL(*mock_client, Write).Times(1);
    EXPECT_CALL(mock_stub, keysRaw).WillOnce(make_releaser(mock_client));

    mp::RemoteSettingsHandler handler{"prefix", mock_stub, &mock_term, 31};

    EXPECT_THAT(handler.keys(), IsEmpty());
}

TEST_F(RemoteSettingsTest, keysReturnsRemoteKeys)
{
    constexpr auto some_keys = std::array{"a.b", "c.d.e", "f"};

    auto mock_client = make_mock_reader_writer<mp::KeysRequest, mp::KeysReply>();

    EXPECT_CALL(*mock_client, Write).Times(1);
    EXPECT_CALL(*mock_client, Read).WillOnce([&some_keys](mp::KeysReply* reply) {
        reply->mutable_settings_keys()->Add(some_keys.begin(), some_keys.end());
        return false;
    });

    EXPECT_CALL(mock_stub, keysRaw).WillOnce(make_releaser(mock_client)); /* grpc code claims ownership of the
                                                                                    ptr that keysRaw() returns */

    mp::RemoteSettingsHandler handler{"prefix", mock_stub, &mock_term, 99};
    EXPECT_THAT(handler.keys(), UnorderedElementsAreArray(some_keys));
}

TEST_F(RemoteSettingsTest, keysReturnsNoKeysWhenDaemonNotAround)
{
    auto mock_client = make_mock_reader_writer<mp::KeysRequest, mp::KeysReply>();

    EXPECT_CALL(*mock_client, Write).Times(1);
    EXPECT_CALL(*mock_client, Finish).WillOnce(Return(grpc::Status{grpc::StatusCode::NOT_FOUND, "remote not around"}));

    EXPECT_CALL(mock_stub, keysRaw).WillOnce(make_releaser(mock_client));

    auto prefix = "remote.";
    mp::RemoteSettingsHandler handler{prefix, mock_stub, &mock_term, 0};

    EXPECT_THAT(handler.keys(), IsEmpty());
}

TEST_F(RemoteSettingsTest, keysThrowsOnOtherErrorFromRemote)
{
    constexpr auto error_code = grpc::StatusCode::INTERNAL;
    constexpr auto error_msg = "unexpected";
    constexpr auto error_details = "details";

    auto mock_client = make_mock_reader_writer<mp::KeysRequest, mp::KeysReply>();

    EXPECT_CALL(*mock_client, Write).Times(1);
    EXPECT_CALL(*mock_client, Finish).WillOnce(Return(grpc::Status{error_code, error_msg, error_details}));

    EXPECT_CALL(mock_stub, keysRaw).WillOnce(make_releaser(mock_client));

    mp::RemoteSettingsHandler handler{"prefix.", mock_stub, &mock_term, 789};
    MP_EXPECT_THROW_THAT(
        handler.keys(), mp::RemoteHandlerException,
        Property(&mp::RemoteHandlerException::get_status, make_status_matcher(error_code, error_msg, error_details)));
}

TEST_F(RemoteSettingsTest, getRequestsSoughtSetting)
{
    constexpr auto prefix = "a.";
    const auto key = QString{prefix} + "key";

    auto mock_client = make_mock_reader_writer<mp::GetRequest, mp::GetReply>();

    EXPECT_CALL(*mock_client, Write(Property(&mp::GetRequest::key, Eq(key.toStdString())), _)).WillOnce(Return(true));
    EXPECT_CALL(mock_stub, getRaw).WillOnce(make_releaser(mock_client));

    mp::RemoteSettingsHandler handler{prefix, mock_stub, &mock_term, 1};
    handler.get(key);
}

TEST_F(RemoteSettingsTest, getReturnsObtainedValue)
{
    constexpr auto prefix = "local.", val = "asdf";

    auto mock_client = make_mock_reader_writer<mp::GetRequest, mp::GetReply>();

    EXPECT_CALL(*mock_client, Write).WillOnce(Return(true));
    EXPECT_CALL(*mock_client, Read).WillOnce([val](mp::GetReply* reply) {
        reply->set_value(val);
        return false;
    });

    EXPECT_CALL(mock_stub, getRaw).WillOnce(make_releaser(mock_client));

    mp::RemoteSettingsHandler handler{prefix, mock_stub, &mock_term, 11};
    EXPECT_THAT(handler.get(QString{prefix} + "key"), Eq(val));
}

TEST_F(RemoteSettingsTest, getThrowsOnWrongPrefix)
{
    constexpr auto prefix = "local.", key = "client.gui.something";

    mp::RemoteSettingsHandler handler{prefix, mock_stub, &mock_term, 2};
    MP_EXPECT_THROW_THAT(handler.get(key), mp::UnrecognizedSettingException, mpt::match_what(HasSubstr(key)));
}

TEST_F(RemoteSettingsTest, getThrowsOnOtherErrorFromRemote)
{
    constexpr auto error_code = grpc::StatusCode::INVALID_ARGUMENT;
    constexpr auto error_msg = "an error";
    constexpr auto error_details = "whatever";

    auto mock_client = make_mock_reader_writer<mp::GetRequest, mp::GetReply>();

    EXPECT_CALL(*mock_client, Write).WillOnce(Return(true));
    EXPECT_CALL(*mock_client, Finish).WillOnce(Return(grpc::Status{error_code, error_msg, error_details}));

    EXPECT_CALL(mock_stub, getRaw).WillOnce(make_releaser(mock_client));

    mp::RemoteSettingsHandler handler{"asdf", mock_stub, &mock_term, 3};
    MP_EXPECT_THROW_THAT(
        handler.get("asdf.asdf"), mp::RemoteHandlerException,
        Property(&mp::RemoteHandlerException::get_status, make_status_matcher(error_code, error_msg, error_details)));
}

TEST_F(RemoteSettingsTest, setRequestsSpecifiedSettingKeyAndValue)
{
    constexpr auto prefix = "remote-", val = "setting-value";
    const auto key = QString{prefix} + "setting-key";

    auto mock_client = make_mock_reader_writer<mp::SetRequest, mp::SetReply>();

    EXPECT_CALL(
        *mock_client,
        Write(AllOf(Property(&mp::SetRequest::key, Eq(key.toStdString())), Property(&mp::SetRequest::val, Eq(val))), _))
        .WillOnce(Return(true));
    EXPECT_CALL(mock_stub, setRaw).WillOnce(make_releaser(mock_client));

    mp::RemoteSettingsHandler handler{prefix, mock_stub, &mock_term, 22};
    handler.set(key, val);
}

TEST_F(RemoteSettingsTest, setThrowsOnWrongPrefix)
{
    constexpr auto prefix = "local.", key = "client.gui.something";

    mp::RemoteSettingsHandler handler{prefix, mock_stub, &mock_term, 2};
    MP_EXPECT_THROW_THAT(handler.set(key, "val"), mp::UnrecognizedSettingException, mpt::match_what(HasSubstr(key)));
}

TEST_F(RemoteSettingsTest, setThrowsOnOtherErrorFromRemote)
{
    constexpr auto error_code = grpc::StatusCode::FAILED_PRECONDITION;
    constexpr auto error_msg = "no worky";
    constexpr auto error_details = "because";

    auto mock_client = make_mock_reader_writer<mp::SetRequest, mp::SetReply>();

    EXPECT_CALL(*mock_client, Write).WillOnce(Return(true));
    EXPECT_CALL(*mock_client, Finish).WillOnce(Return(grpc::Status{error_code, error_msg, error_details}));

    EXPECT_CALL(mock_stub, setRaw).WillOnce(make_releaser(mock_client));

    mp::RemoteSettingsHandler handler{"a", mock_stub, &mock_term, 33};
    MP_EXPECT_THROW_THAT(
        handler.set("a.ongajgsiffsgu", "dfoinig"), mp::RemoteHandlerException,
        Property(&mp::RemoteHandlerException::get_status, make_status_matcher(error_code, error_msg, error_details)));
}

} // namespace
