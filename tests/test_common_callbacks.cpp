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

enum class CommonCallbackType
{
    Logging,
    Reply,
    Iterative
};

struct TestLoggingSpinnerCallbacks : public TestSpinnerCallbacks, public WithParamInterface<CommonCallbackType>
{
    std::function<void(const mp::MountReply&, grpc::ClientReaderWriterInterface<mp::MountRequest, mp::MountReply>*)>
    make_callback()
    {
        switch (GetParam())
        {
        case CommonCallbackType::Logging:
            return mp::make_logging_spinner_callback<mp::MountRequest, mp::MountReply>(spinner, err);
            break;
        case CommonCallbackType::Reply:
            return mp::make_reply_spinner_callback<mp::MountRequest, mp::MountReply>(spinner, err);
            break;
        case CommonCallbackType::Iterative:
            return mp::make_iterative_spinner_callback<mp::MountRequest, mp::MountReply>(spinner, term);
            break;
        default:
            assert(false && "shouldn't be here");
        }
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

INSTANTIATE_TEST_SUITE_P(TestLoggingSpinnerCallbacks,
                         TestLoggingSpinnerCallbacks,
                         Values(CommonCallbackType::Logging, CommonCallbackType::Reply, CommonCallbackType::Iterative));

struct TestReplySpinnerCallbacks : public TestSpinnerCallbacks, public WithParamInterface<CommonCallbackType>
{
    std::function<void(const mp::MountReply&, grpc::ClientReaderWriterInterface<mp::MountRequest, mp::MountReply>*)>
    make_callback()
    {
        switch (GetParam())
        {
        case CommonCallbackType::Reply:
            return mp::make_reply_spinner_callback<mp::MountRequest, mp::MountReply>(spinner, err);
            break;
        case CommonCallbackType::Iterative:
            return mp::make_iterative_spinner_callback<mp::MountRequest, mp::MountReply>(spinner, term);
            break;
        default:
            assert(false && "shouldn't be here");
            throw std::runtime_error{"bad test instantiation"};
        }
    }
};

TEST_P(TestReplySpinnerCallbacks, replySpinnerCallbackUpdatesSpinnerMessage)
{
    constexpr auto msg = "answer";

    mp::MountReply reply;
    reply.set_reply_message(msg);

    make_callback()(reply, nullptr);

    EXPECT_THAT(err.str(), IsEmpty());
    EXPECT_THAT(out.str(), HasSubstr(msg));
}

TEST_P(TestReplySpinnerCallbacks, replySpinnerCallbackIgnoresEmptyMessage)
{
    mp::MountReply reply;

    make_callback()(reply, nullptr);

    EXPECT_THAT(err.str(), IsEmpty());
    EXPECT_THAT(out.str(), IsEmpty());
}

INSTANTIATE_TEST_SUITE_P(TestReplySpinnerCallbacks,
                         TestReplySpinnerCallbacks,
                         Values(CommonCallbackType::Reply, CommonCallbackType::Iterative));

TEST_F(TestSpinnerCallbacks, iterativeSpinnerCallbackHandlesPasswordRequest)
{
    constexpr auto pwd = "xyz";
    auto [mock_client_platform, guard] = mpt::MockClientPlatform::inject<StrictMock>();
    mpt::MockClientReaderWriter<mp::RestartRequest, mp::RestartReply> mock_client;

    mp::RestartReply reply;
    reply.set_password_requested(true);

    EXPECT_CALL(*mock_client_platform, get_password(&term)).WillOnce(Return(pwd));
    EXPECT_CALL(mock_client, Write(Property(&mp::RestartRequest::password, StrEq(pwd)), _)).WillOnce(Return(true));

    auto cb = mp::make_iterative_spinner_callback<mp::RestartRequest, mp::RestartReply>(spinner, term);
    cb(reply, &mock_client);

    EXPECT_THAT(err.str(), IsEmpty());
    EXPECT_THAT(out.str(), clearStreamMatcher());
}

TEST_F(TestSpinnerCallbacks, confirmationCallbackAnswers)
{
    constexpr auto key = "local.instance-name.bridged";

    mpt::MockClientReaderWriter<mp::SetRequest, mp::SetReply> mock_client;

    mp::SetReply reply;
    reply.set_needs_authorization(true);

    EXPECT_CALL(mock_client, Write(_, _)).WillOnce(Return(true));

    auto callback = mp::make_confirmation_callback<mp::SetRequest, mp::SetReply>(term, key);
    callback(reply, &mock_client);

    EXPECT_THAT(err.str(), IsEmpty());
    EXPECT_THAT(out.str(), clearStreamMatcher());
}
