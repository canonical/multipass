/*
 * Copyright (C) 2021 Canonical, Ltd.
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

#include "client_test_fixture.h"

#include <tests/mock_utils.h>

#include <multipass/constants.h>

#include <chrono>
#include <thread>

namespace mp = multipass;
namespace mpt = multipass::test;
using namespace testing;

namespace
{
const std::vector<std::string> timeout_commands{"launch", "start", "restart", "shell"};
const std::vector<std::string> valid_timeouts{"120", "1234567"};
const std::vector<std::string> invalid_timeouts{"-1", "0", "a", "3min", "15.51", ""};

struct TimeoutCorrectSuite : mpt::ClientTestFixture, WithParamInterface<std::tuple<std::string, std::string>>
{
};

struct TimeoutNullSuite : mpt::ClientTestFixture, WithParamInterface<std::string>
{
};

struct TimeoutInvalidSuite : mpt::ClientTestFixture, WithParamInterface<std::tuple<std::string, std::string>>
{
};

struct TimeoutSuite : mpt::ClientTestFixture, WithParamInterface<std::string>
{
    void SetUp() override
    {
        ON_CALL(mock_daemon, launch).WillByDefault(request_sleeper<mp::LaunchRequest, mp::LaunchReply>);
        ON_CALL(mock_daemon, start).WillByDefault(request_sleeper<mp::StartRequest, mp::StartReply>);
        ON_CALL(mock_daemon, restart).WillByDefault(request_sleeper<mp::RestartRequest, mp::RestartReply>);
        ON_CALL(mock_daemon, ssh_info).WillByDefault(request_sleeper<mp::SSHInfoRequest, mp::SSHInfoReply>);
    }

    template <typename RequestType, typename ReplyType>
    static grpc::Status request_sleeper(grpc::ServerContext* context, const RequestType* request,
                                        grpc::ServerWriter<ReplyType>* response)
    {
        std::this_thread::sleep_for(std::chrono::seconds(2));
        return grpc::Status::OK;
    }
};
} // namespace

TEST_P(TimeoutCorrectSuite, cmds_with_timeout_ok)
{
    const auto& [command, timeout] = GetParam();

    EXPECT_CALL(mock_daemon, launch).Times(AtMost(1));
    EXPECT_CALL(mock_daemon, start).Times(AtMost(1));
    EXPECT_CALL(mock_daemon, restart).Times(AtMost(1));
    EXPECT_CALL(mock_daemon, ssh_info).Times(AtMost(1));
    EXPECT_THAT(send_command({command, "--timeout", timeout}), Eq(mp::ReturnCode::Ok));
}

INSTANTIATE_TEST_SUITE_P(Client, TimeoutCorrectSuite, Combine(ValuesIn(timeout_commands), ValuesIn(valid_timeouts)));

TEST_P(TimeoutNullSuite, cmds_with_timeout_null_bad)
{
    EXPECT_THAT(send_command({GetParam(), "--timeout"}), Eq(mp::ReturnCode::CommandLineError));
}

INSTANTIATE_TEST_SUITE_P(Client, TimeoutNullSuite, ValuesIn(timeout_commands));

TEST_P(TimeoutInvalidSuite, cmds_with_invalid_timeout_bad)
{
    std::stringstream cerr_stream;
    const auto& [command, timeout] = GetParam();

    EXPECT_THAT(send_command({command, "--timeout", timeout}, trash_stream, cerr_stream),
                Eq(mp::ReturnCode::CommandLineError));

    EXPECT_EQ(cerr_stream.str(), "error: --timeout value has to be a positive integer\n");
}

INSTANTIATE_TEST_SUITE_P(Client, TimeoutInvalidSuite, Combine(ValuesIn(timeout_commands), ValuesIn(invalid_timeouts)));

TEST_P(TimeoutSuite, command_exits_on_timeout)
{
    auto [mock_utils, guard] = mpt::MockUtils::inject();

    EXPECT_CALL(mock_daemon, launch).Times(AtMost(1));
    EXPECT_CALL(mock_daemon, start).Times(AtMost(1));
    EXPECT_CALL(mock_daemon, restart).Times(AtMost(1));
    EXPECT_CALL(mock_daemon, ssh_info).Times(AtMost(1));
    EXPECT_CALL(*mock_utils, exit(mp::timeout_exit_code));

    send_command({GetParam(), "--timeout", "1"});
}

TEST_P(TimeoutSuite, command_completes_without_timeout)
{
    EXPECT_CALL(mock_daemon, launch).Times(AtMost(1));
    EXPECT_CALL(mock_daemon, start).Times(AtMost(1));
    EXPECT_CALL(mock_daemon, restart).Times(AtMost(1));
    EXPECT_CALL(mock_daemon, ssh_info).Times(AtMost(1));

    EXPECT_EQ(send_command({GetParam(), "--timeout", "5"}), mp::ReturnCode::Ok);
}

INSTANTIATE_TEST_SUITE_P(Client, TimeoutSuite, ValuesIn(timeout_commands));
