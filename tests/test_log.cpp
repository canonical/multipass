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
    ASSERT_STREQ(mpl::as_string(mpl::Level::debug).c_str(), "debug");
    ASSERT_STREQ(mpl::as_string(mpl::Level::error).c_str(), "error");
    ASSERT_STREQ(mpl::as_string(mpl::Level::info).c_str(), "info");
    ASSERT_STREQ(mpl::as_string(mpl::Level::warning).c_str(), "warning");
    ASSERT_STREQ(mpl::as_string(mpl::Level::trace).c_str(), "trace");
    ASSERT_STREQ(mpl::as_string(static_cast<mpl::Level>(-1)).c_str(), "unknown");
    ASSERT_STREQ(mpl::as_string(static_cast<mpl::Level>(5)).c_str(), "unknown");
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
