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

#include <src/client/cli/cmd/common_callbacks.h>

#include <sstream>

namespace mp = multipass;
using namespace testing;

TEST(CommonCallbacks, logging_spinner_callback_logs)
{
    std::ostringstream out, err;
    mp::AnimatedSpinner spinner{out};
    constexpr auto log = "message in a bottle";

    mp::PurgeReply reply;
    reply.set_log_line(log);

    auto cb = mp::make_logging_spinner_callback<mp::PurgeRequest, mp::PurgeReply>(spinner, err);
    cb(reply, nullptr);

    EXPECT_THAT(err.str(), StrEq(log));
    EXPECT_THAT(out.str(), MatchesRegex(R"(\s*)")); /* this is not empty because print stops, stop clears, and clear
                                                       prints carriage returns and spaces */
}
