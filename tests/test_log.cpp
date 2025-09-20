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

#include "mock_logger.h"

#include <gtest/gtest.h>
#include <multipass/logging/level.h>

namespace mpl = multipass::logging;
namespace mpt = multipass::test;

struct LogTests : ::testing::Test
{
    mpt::MockLogger::Scope logger_scope = mpt::MockLogger::inject();
};

TEST_F(LogTests, testLevelsAsString)
{
    ASSERT_EQ(mpl::as_string(mpl::Level::debug), "debug");
    ASSERT_EQ(mpl::as_string(mpl::Level::error), "error");
    ASSERT_EQ(mpl::as_string(mpl::Level::info), "info");
    ASSERT_EQ(mpl::as_string(mpl::Level::warning), "warning");
    ASSERT_EQ(mpl::as_string(mpl::Level::trace), "trace");
    ASSERT_EQ(mpl::as_string(static_cast<mpl::Level>(-1)), "unknown");
    ASSERT_EQ(mpl::as_string(static_cast<mpl::Level>(5)), "unknown");
}

TEST_F(LogTests, testNonFormatOverload)
{
    logger_scope.mock_logger->expect_log(mpl::Level::error, "no format whatsoever {}");
    mpl::log(mpl::Level::error, "test_category", "no format whatsoever {}");
}

TEST_F(LogTests, testFormatOverloadSingleArg)
{
    logger_scope.mock_logger->expect_log(mpl::Level::error, "with formatting 1");
    mpl::log(mpl::Level::error, "test_category", "with formatting {}", 1);
}

TEST_F(LogTests, testFormatOverloadMultipleArgs)
{
    logger_scope.mock_logger->expect_log(mpl::Level::error, "with formatting 1 test");
    mpl::log(mpl::Level::error, "test_category", "with formatting {} {}", 1, "test");
}

TEST_F(LogTests, testFormatOverloadMultipleArgsSuperfluous)
{
    logger_scope.mock_logger->expect_log(mpl::Level::error, "with formatting 1 test");
    // Superfluous arguments are ignored. This should be an error with C++20
    mpl::log(mpl::Level::error, "test_category", "with formatting {} {}", 1, "test", "superfluous");
}

TEST_F(LogTests, testFormatOverloadMultipleArgsMissing)
{
    // Missing arguments when using a runtime format string are a runtime error.
    EXPECT_THROW(
        { mpl::log(mpl::Level::error, "test_category", fmt::runtime("with formatting {} {}"), 1); },
        fmt::format_error);
}

// ------------------------------------------------------------------------------

TEST_F(LogTests, testLogErrorFunction)
{
    logger_scope.mock_logger->expect_log(mpl::Level::error, "with formatting 1");
    mpl::error("test_category", "with formatting {}", 1);
}

TEST_F(LogTests, testLogWarnFunction)
{
    logger_scope.mock_logger->expect_log(mpl::Level::warning, "with formatting 1");
    mpl::warn("test_category", "with formatting {}", 1);
}

TEST_F(LogTests, testLogInfoFunction)
{
    logger_scope.mock_logger->expect_log(mpl::Level::info, "with formatting 1");
    mpl::info("test_category", "with formatting {}", 1);
}

TEST_F(LogTests, testLogDebugFunction)
{
    logger_scope.mock_logger->expect_log(mpl::Level::debug, "with formatting 1");
    mpl::debug("test_category", "with formatting {}", 1);
}

TEST_F(LogTests, testLogTraceFunction)
{
    logger_scope.mock_logger->expect_log(mpl::Level::trace, "with formatting 1");
    mpl::trace("test_category", "with formatting {}", 1);
}
