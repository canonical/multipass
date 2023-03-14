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
 */

#include "common.h"
#include "mock_client_platform.h"
#include "mock_client_rpc.h"
#include "stub_terminal.h"

#include <src/client/cli/cmd/animated_spinner.h>
#include <src/client/cli/cmd/common_callbacks.h>

#include <regex>
#include <sstream>

namespace mp = multipass;
namespace mpt = mp::test;
using namespace testing;

struct TestSpinnerCallbacks : public Test
{
    auto clearStreamMatcher()
    {
        static const std::regex clear_regex{
            R"(\u001B\[2K\u001B\[0A\u001B\[0E|^$)"}; /* A "clear" stream should have nothing on the current
                                     line and the cursor in the leftmost position */
        return Truly([](const auto& str) {
            return std::regex_match(str, clear_regex); // (gtest regex not cutting it on macOS)
        });
    }

    std::ostringstream out, err;
    std::istringstream in;
    mpt::StubTerminal term{out, err, in};
    mp::AnimatedSpinner spinner{out};
};

using IterativeCallback = bool;
struct TestLoggingSpinnerCallbacks : public TestSpinnerCallbacks, public WithParamInterface<IterativeCallback>
{
    std::function<void(const mp::MountReply&, grpc::ClientReaderWriterInterface<mp::MountRequest, mp::MountReply>*)>
    make_callback()
    {
        if (GetParam())
            return mp::make_iterative_spinner_callback<mp::MountRequest, mp::MountReply>(spinner, term);
        else
            return mp::make_logging_spinner_callback<mp::MountRequest, mp::MountReply>(spinner, err);
    }
};

TEST_P(TestLoggingSpinnerCallbacks, loggingSpinnerCallbackLogs)
{
    constexpr auto log = "message in a bottle";

    mp::MountReply reply;
    reply.set_log_line(log);

    make_callback()(reply, nullptr);

    EXPECT_THAT(err.str(), StrEq(log));
    EXPECT_THAT(out.str(), clearStreamMatcher()); /* this is not empty because print stops, stop clears, and clear
                                                     prints ANSI escape characters to clear the line */
}

TEST_P(TestLoggingSpinnerCallbacks, loggingSpinnerCallbackIgnoresEmptyLog)
{
    mp::MountReply reply;
    make_callback()(reply, nullptr);

    EXPECT_THAT(err.str(), IsEmpty());
    EXPECT_THAT(out.str(), clearStreamMatcher());
}

INSTANTIATE_TEST_SUITE_P(TestLoggingSpinnerCallbacks, TestLoggingSpinnerCallbacks, Values(false, true));

TEST_F(TestSpinnerCallbacks, iterativeSpinnerCallbackUpdatesSpinnerMessage)
{
    constexpr auto msg = "answer";

    mp::MountReply reply;
    reply.set_reply_message(msg);

    auto cb = mp::make_iterative_spinner_callback<mp::MountRequest, mp::MountReply>(spinner, term);
    cb(reply, nullptr);

    EXPECT_THAT(err.str(), IsEmpty());
    EXPECT_THAT(out.str(), HasSubstr(msg));
}

TEST_F(TestSpinnerCallbacks, iterativeSpinnerCallbackIgnoresEmptyMessage)
{
    mp::StartReply reply;

    auto cb = mp::make_iterative_spinner_callback<mp::StartRequest, mp::StartReply>(spinner, term);
    cb(reply, nullptr);

    EXPECT_THAT(err.str(), IsEmpty());
    EXPECT_THAT(out.str(), IsEmpty());
}

TEST_F(TestSpinnerCallbacks, iterativeSpinnerCallbackHandlesCredentialRequest)
{
    constexpr auto usr = "ubuntu", pwd = "xyz";
    auto [mock_client_platform, guard] = mpt::MockClientPlatform::inject<StrictMock>();
    mpt::MockClientReaderWriter<mp::RestartRequest, mp::RestartReply> mock_client;

    mp::RestartReply reply;
    reply.set_credentials_requested(true);

    EXPECT_CALL(*mock_client_platform, get_user_password(&term)).WillOnce(Return(std::pair{usr, pwd}));
    EXPECT_CALL(mock_client, Write(Property(&mp::RestartRequest::user_credentials,
                                            AllOf(Property(&mp::UserCredentials::username, StrEq(usr)),
                                                  Property(&mp::UserCredentials::password, StrEq(pwd)))),
                                   _))
        .WillOnce(Return(true));

    auto cb = mp::make_iterative_spinner_callback<mp::RestartRequest, mp::RestartReply>(spinner, term);
    cb(reply, &mock_client);

    EXPECT_THAT(err.str(), IsEmpty());
    EXPECT_THAT(out.str(), clearStreamMatcher());
}
