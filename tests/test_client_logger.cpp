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
using testing::HasSubstr;
using testing::Return;

struct ClientLoggerTests : ::testing::Test
{
    mpl::MultiplexingLogger stub_multiplexing_logger{std::make_unique<mpt::StubLogger>()};
    mpt::MockServerReaderWriter<StubReply, StubReply> mock_srw;
};

TEST_F(ClientLoggerTests, callLog)
{
    EXPECT_CALL(mock_srw,
                Write(Field(&StubReply::stored_msg, HasSubstr("[debug] [cat] msg")), testing::_))
        .WillOnce(Return(true));
    uut_t logger{mpl::Level::debug, stub_multiplexing_logger, &mock_srw};
    logger.log(mpl::Level::debug, "cat", "msg");
}

TEST_F(ClientLoggerTests, callLogFiltered)
{
    EXPECT_CALL(mock_srw, Write(testing::_, testing::_)).Times(0);
    uut_t logger{mpl::Level::debug, stub_multiplexing_logger, &mock_srw};
    logger.log(mpl::Level::trace, "cat", "msg");
}
