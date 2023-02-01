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
#include "daemon_test_fixture.h"
#include "mock_platform.h"
#include "mock_server_reader_writer.h"
#include "mock_settings.h"
#include "mock_utils.h"

#include <multipass/constants.h>

namespace mp = multipass;
namespace mpt = multipass::test;

using namespace testing;

namespace
{
constexpr auto saved_hash{"f28cb995d91eed8064674766f28e468aae8065"};
constexpr auto good_hash{"f28cb995d91eed8064674766f28e468aae8065"};
constexpr auto bad_hash{"b2cf02af556c857dd77de2d2476f3830fd0214"};

struct TestDaemonAuthenticate : public mpt::DaemonTestFixture
{
    TestDaemonAuthenticate()
    {
        EXPECT_CALL(mock_settings, register_handler(_)).WillRepeatedly(Return(nullptr));
        EXPECT_CALL(mock_settings, unregister_handler).Times(AnyNumber());
    }

    mpt::MockUtils::GuardedMock utils_attr{mpt::MockUtils::inject<NiceMock>()};
    mpt::MockUtils* mock_utils = utils_attr.first;
    mpt::MockPlatform::GuardedMock platform_attr{mpt::MockPlatform::inject<NiceMock>()};
    mpt::MockPlatform* mock_platform = platform_attr.first;

    mpt::MockSettings::GuardedMock mock_settings_injection = mpt::MockSettings::inject();
    mpt::MockSettings& mock_settings = *mock_settings_injection.first;
};
} // namespace

TEST_F(TestDaemonAuthenticate, authenticateNoErrorReturnsOk)
{
    mp::Daemon daemon{config_builder.build()};

    EXPECT_CALL(mock_settings, get(Eq(mp::passphrase_key))).WillOnce(Return(saved_hash));

    EXPECT_CALL(*mock_utils, generate_scrypt_hash_for(Eq(QString("foo")))).WillOnce(Return(good_hash));

    mp::AuthenticateRequest request;
    request.set_passphrase("foo");

    auto status =
        call_daemon_slot(daemon, &mp::Daemon::authenticate, request,
                         StrictMock<mpt::MockServerReaderWriter<mp::AuthenticateReply, mp::AuthenticateRequest>>{});

    EXPECT_TRUE(status.ok());
}

TEST_F(TestDaemonAuthenticate, authenticateNoPassphraseSetReturnsError)
{
    mp::Daemon daemon{config_builder.build()};

    EXPECT_CALL(mock_settings, get(Eq(mp::passphrase_key)));

    mp::AuthenticateRequest request;
    request.set_passphrase("foo");

    auto status =
        call_daemon_slot(daemon, &mp::Daemon::authenticate, request,
                         StrictMock<mpt::MockServerReaderWriter<mp::AuthenticateReply, mp::AuthenticateRequest>>{});

    EXPECT_EQ(status.error_code(), grpc::StatusCode::FAILED_PRECONDITION);
    EXPECT_EQ(status.error_message(),
              "Passphrase is not set. Please `multipass set local.passphrase` with a trusted client.");
}

TEST_F(TestDaemonAuthenticate, authenticatePassphraseMismatchReturnsError)
{
    mp::Daemon daemon{config_builder.build()};

    EXPECT_CALL(mock_settings, get(Eq(mp::passphrase_key))).WillOnce(Return(saved_hash));

    EXPECT_CALL(*mock_utils, generate_scrypt_hash_for(Eq(QString("foo")))).WillOnce(Return(bad_hash));

    mp::AuthenticateRequest request;
    request.set_passphrase("foo");

    auto status =
        call_daemon_slot(daemon, &mp::Daemon::authenticate, request,
                         StrictMock<mpt::MockServerReaderWriter<mp::AuthenticateReply, mp::AuthenticateRequest>>{});

    EXPECT_EQ(status.error_code(), grpc::StatusCode::INVALID_ARGUMENT);
    EXPECT_EQ(status.error_message(), "Passphrase is not correct. Please try again.");
}

TEST_F(TestDaemonAuthenticate, authenticateCatchesExceptionReturnsError)
{
    const std::string error_msg{"Getting settings failed"};
    mp::Daemon daemon{config_builder.build()};

    EXPECT_CALL(mock_settings, get(Eq(mp::passphrase_key))).WillOnce(Throw(std::runtime_error(error_msg)));

    mp::AuthenticateRequest request;
    request.set_passphrase("foo");

    auto status =
        call_daemon_slot(daemon, &mp::Daemon::authenticate, request,
                         StrictMock<mpt::MockServerReaderWriter<mp::AuthenticateReply, mp::AuthenticateRequest>>{});

    EXPECT_EQ(status.error_code(), grpc::StatusCode::INTERNAL);
    EXPECT_EQ(status.error_message(), error_msg);
}
