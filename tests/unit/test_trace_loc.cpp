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
#include "mock_logger.h"

#include <multipass/logging/trace_location.h>

namespace mpl = multipass::logging;
namespace mpt = multipass::test;
using namespace testing;

namespace
{
constexpr auto test_category = "test_category";

struct LogLocationTests : Test
{
    mpt::MockLogger::Scope logger_scope = mpt::MockLogger::inject(mpl::Level::trace);
};

TEST_F(LogLocationTests, logsWithSourceLocation)
{
    constexpr auto level = mpl::Level::debug;
    logger_scope.mock_logger->expect_log(level, "test_trace_loc.cpp");
    mpl::log_location(level, test_category, "blarg");
}

TEST_F(LogLocationTests, logsWithFormatArgs)
{
    constexpr auto level = mpl::Level::info;
    logger_scope.mock_logger->expect_log(level, "value is 42.");
    mpl::log_location(level, test_category, "value is {}.", 42);
}

TEST_F(LogLocationTests, logsWithMultipleFormatArgs)
{
    constexpr auto level = mpl::Level::warning;
    logger_scope.mock_logger->expect_log(level, "values: 1, hello, 3.14");
    mpl::log_location(level, test_category, "values: {}, {}, {}", 1, "hello", 3.14);
}

TEST_F(LogLocationTests, includesFunctionName)
{
    constexpr auto level = mpl::Level::error;
    logger_scope.mock_logger->expect_log(level, "TestBody");
    mpl::log_location(level, test_category, "msg");
}

TEST_F(LogLocationTests, includesLineNumber)
{
    constexpr auto level = mpl::Level::trace;
    constexpr int expected_line = __LINE__ + 2;
    logger_scope.mock_logger->expect_log(level, std::to_string(expected_line));
    mpl::log_location(level, test_category, "msg");
}

TEST_F(LogLocationTests, traceLocationLogsAtTraceLevel)
{
    constexpr auto level = mpl::Level::trace;
    constexpr auto msg = "trace message";
    logger_scope.mock_logger->expect_log(level, msg);
    mpl::trace_location(test_category, msg);
}

TEST_F(LogLocationTests, debugLocationLogsAtDebugLevel)
{
    constexpr auto level = mpl::Level::debug;
    constexpr auto msg = "debug message";
    logger_scope.mock_logger->expect_log(level, msg);
    mpl::debug_location(test_category, msg);
}
} // namespace
