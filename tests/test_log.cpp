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

#include <gtest/gtest.h>
#include <multipass/logging/level.h>

namespace mpl = multipass::logging;

struct log_tests : ::testing::Test
{
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
