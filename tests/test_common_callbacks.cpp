/*
 * Copyright (C) 2023 Canonical, Ltd.
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
#include "stub_terminal.h"

#include <src/client/cli/cmd/animated_spinner.h>
#include <src/client/cli/cmd/common_callbacks.h>

#include <sstream>

namespace mp = multipass;
namespace mpt = mp::test;
using namespace testing;

struct TestSpinnerCallbacks : public Test
{
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

TEST_P(TestLoggingSpinnerCallbacks, logging_spinner_callback_logs)
{
    constexpr auto log = "message in a bottle";

    mp::MountReply reply;
    reply.set_log_line(log);

    make_callback()(reply, nullptr);

    EXPECT_THAT(err.str(), StrEq(log));
    EXPECT_THAT(out.str(), MatchesRegex(R"(\s*)")); /* this is not empty because print stops, stop clears, and clear
                                                       prints carriage returns and spaces */
}

TEST_P(TestLoggingSpinnerCallbacks, logging_spinner_callback_ignores_empty_log)
{
    mp::MountReply reply;
    make_callback()(reply, nullptr);

    EXPECT_THAT(err.str(), IsEmpty());
    EXPECT_THAT(out.str(), MatchesRegex(R"(\s*)")); /* this is not empty because print stops, stop clears, and clear
                                                       prints carriage returns and spaces */
}

INSTANTIATE_TEST_SUITE_P(TestLoggingSpinnerCallbacks, TestLoggingSpinnerCallbacks, Values(false, true));

TEST_F(TestSpinnerCallbacks, iterative_spinner_callback_updates_spinner_message)
{
    constexpr auto msg = "answer";

    mp::MountReply reply;
    reply.set_reply_message(msg);

    auto cb = mp::make_iterative_spinner_callback<mp::MountRequest, mp::MountReply>(spinner, term);
    cb(reply, nullptr);

    EXPECT_THAT(err.str(), IsEmpty());
    EXPECT_THAT(out.str(), HasSubstr(msg));
}
