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

struct log_tests : ::testing::Test
{
    mpt::MockLogger::Scope logger_scope = mpt::MockLogger::inject();
};

TEST_F(log_tests, test_levels_as_string)
{
    ASSERT_EQ(mpl::as_string(mpl::Level::debug), "debug");
    ASSERT_EQ(mpl::as_string(mpl::Level::error), "error");
    ASSERT_EQ(mpl::as_string(mpl::Level::info), "info");
    ASSERT_EQ(mpl::as_string(mpl::Level::warning), "warning");
    ASSERT_EQ(mpl::as_string(mpl::Level::trace), "trace");
    ASSERT_EQ(mpl::as_string(static_cast<mpl::Level>(-1)), "unknown");
    ASSERT_EQ(mpl::as_string(static_cast<mpl::Level>(5)), "unknown");
}

TEST_F(log_tests, test_non_format_overload)
{
    logger_scope.mock_logger->expect_log(mpl::Level::error, "no format whatsoever {}");
    mpl::log(mpl::Level::error, "test_category", "no format whatsoever {}");
}

TEST_F(log_tests, test_format_overload_single_arg)
{
    logger_scope.mock_logger->expect_log(mpl::Level::error, "with formatting 1");
    mpl::log(mpl::Level::error, "test_category", "with formatting {}", 1);
}

TEST_F(log_tests, test_format_overload_multiple_args)
{
    logger_scope.mock_logger->expect_log(mpl::Level::error, "with formatting 1 test");
    mpl::log(mpl::Level::error, "test_category", "with formatting {} {}", 1, "test");
}

TEST_F(log_tests, test_format_overload_multiple_args_superfluous)
{
    logger_scope.mock_logger->expect_log(mpl::Level::error, "with formatting 1 test");
    // Superfluous arguments are ignored. This should be an error with C++20
    mpl::log(mpl::Level::error, "test_category", "with formatting {} {}", 1, "test", "superfluous");
}

TEST_F(log_tests, test_format_overload_multiple_args_missing)
{
    // Missing arguments are runtime error in C++17. This should be a compile
    // time error with the C++20
    EXPECT_THROW({ mpl::log(mpl::Level::error, "test_category", "with formatting {} {}", 1); }, fmt::format_error);
}

// ------------------------------------------------------------------------------

TEST_F(log_tests, test_log_error_function)
{
    logger_scope.mock_logger->expect_log(mpl::Level::error, "with formatting 1");
    mpl::error("test_category", "with formatting {}", 1);
}

TEST_F(log_tests, test_log_warn_function)
{
    logger_scope.mock_logger->expect_log(mpl::Level::warning, "with formatting 1");
    mpl::warn("test_category", "with formatting {}", 1);
}

TEST_F(log_tests, test_log_info_function)
{
    logger_scope.mock_logger->expect_log(mpl::Level::info, "with formatting 1");
    mpl::info("test_category", "with formatting {}", 1);
}

TEST_F(log_tests, test_log_debug_function)
{
    logger_scope.mock_logger->expect_log(mpl::Level::debug, "with formatting 1");
    mpl::debug("test_category", "with formatting {}", 1);
}

TEST_F(log_tests, test_log_trace_function)
{
    logger_scope.mock_logger->expect_log(mpl::Level::trace, "with formatting 1");
    mpl::trace("test_category", "with formatting {}", 1);
}

// ------------------------------------------------------------------------------

TEST_F(log_tests, test_log_error_function_noargs)
{
    logger_scope.mock_logger->expect_log(mpl::Level::error, "without formatting {}");
    mpl::error("test_category", "without formatting {}");
}

TEST_F(log_tests, test_log_warn_function_noargs)
{
    logger_scope.mock_logger->expect_log(mpl::Level::warning, "without formatting {}");
    mpl::warn("test_category", "without formatting {}");
}

TEST_F(log_tests, test_log_info_function_noargs)
{
    logger_scope.mock_logger->expect_log(mpl::Level::info, "without formatting {}");
    mpl::info("test_category", "without formatting {}");
}

TEST_F(log_tests, test_log_debug_function_noargs)
{
    logger_scope.mock_logger->expect_log(mpl::Level::debug, "without formatting {}");
    mpl::debug("test_category", "without formatting {}");
}

TEST_F(log_tests, test_log_trace_function_noargs)
{
    logger_scope.mock_logger->expect_log(mpl::Level::trace, "without formatting {}");
    mpl::trace("test_category", "without formatting {}");
}
