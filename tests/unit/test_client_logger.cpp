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
#include "mock_server_reader_writer.h"
#include "stub_logger.h"

#include <multipass/logging/client_logger.h>
#include <multipass/logging/level.h>
#include <multipass/logging/multiplexing_logger.h>

#include <chrono>
#include <condition_variable>
#include <latch>
#include <mutex>
#include <thread>

namespace mpl = multipass::logging;
namespace mpt = multipass::test;

struct StubReply
{
    void set_log_line(const std::string& msg)
    {
        stored_msg = msg;
    }

    std::string stored_msg;
};

using uut_t = mpl::ClientLogger<StubReply, StubReply>;
using namespace testing;

struct ClientLoggerTests : Test
{
    mpl::MultiplexingLogger stub_multiplexing_logger{std::make_unique<mpt::StubLogger>()};
    mpt::MockServerReaderWriter<StubReply, StubReply> mock_srw;
};

TEST_F(ClientLoggerTests, callLog)
{
    EXPECT_CALL(mock_srw, Write(Field(&StubReply::stored_msg, HasSubstr("[debug] [cat] msg")), _))
        .WillOnce(Return(true));
    uut_t logger{mpl::Level::debug, stub_multiplexing_logger, &mock_srw};
    logger.log(mpl::Level::debug, "cat", "msg");
}

TEST_F(ClientLoggerTests, callLogFiltered)
{
    EXPECT_CALL(mock_srw, Write).Times(0);
    uut_t logger{mpl::Level::debug, stub_multiplexing_logger, &mock_srw};
    logger.log(mpl::Level::trace, "cat", "msg");
}

TEST_F(ClientLoggerTests, concurrentLogsDoNotOverlapWrites)
{
    using namespace std::chrono_literals;

    std::mutex mutex;
    std::condition_variable cv;
    int in_flight = 0;
    bool overlapped = false;

    EXPECT_CALL(mock_srw, Write).Times(2).WillRepeatedly([&] {
        std::unique_lock lock{mutex};
        if (++in_flight > 1)
            overlapped = true;
        cv.notify_all();
        cv.wait_for(lock, 100ms, [&] { return overlapped; });
        --in_flight;
        return true;
    });

    uut_t logger{mpl::Level::debug, stub_multiplexing_logger, &mock_srw};

    std::latch start{2};
    auto log_once = [this, &start] {
        start.arrive_and_wait();
        stub_multiplexing_logger.log(mpl::Level::debug, "cat", "msg");
    };

    std::thread first{log_once};
    std::thread second{log_once};
    first.join();
    second.join();

    EXPECT_FALSE(overlapped);
}
