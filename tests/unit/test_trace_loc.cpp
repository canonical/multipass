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

#include <multipass/logging/trace_loc.h>

namespace mpl = multipass::logging;
namespace mpt = multipass::test;
using namespace testing;

namespace
{
struct TraceLocTests : Test
{
    mpt::MockLogger::Scope logger_scope = mpt::MockLogger::inject(mpl::Level::trace);
};

TEST_F(TraceLocTests, logsWithSourceLocation)
{
    logger_scope.mock_logger->expect_log(mpl::Level::trace, "test_trace_loc.cpp");
    mpl::trace_loc("test_category", "blarg");
}

TEST_F(TraceLocTests, logsWithFormatArgs)
{
    logger_scope.mock_logger->expect_log(mpl::Level::trace, "value is 42.");
    mpl::trace_loc("test_category", "value is {}.", 42);
}

TEST_F(TraceLocTests, logsWithMultipleFormatArgs)
{
    logger_scope.mock_logger->expect_log(mpl::Level::trace, "values: 1, hello, 3.14");
    mpl::trace_loc("test_category", "values: {}, {}, {}", 1, "hello", 3.14);
}

TEST_F(TraceLocTests, includesFunctionName)
{
    logger_scope.mock_logger->expect_log(mpl::Level::trace, "TestBody");
    mpl::trace_loc("test_category", "checking function name");
}

TEST_F(TraceLocTests, includesLineNumber)
{
    constexpr int expected_line = __LINE__ + 2;
    logger_scope.mock_logger->expect_log(mpl::Level::trace, std::to_string(expected_line));
    mpl::trace_loc("test_category", "checking line number");
}
} // namespace
