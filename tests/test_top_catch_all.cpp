/*
 * Copyright (C) 2020 Canonical, Ltd.
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

#include <multipass/top_catch_all.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <functional>

namespace mp = multipass;

TEST(TopCatchAll, calls_function_with_no_args)
{
    int ret = 123, got = 0;
    EXPECT_NO_THROW(got = mp::top_catch_all("", [ret] { return ret; }););
    EXPECT_EQ(got, ret);
}

TEST(TopCatchAll, calls_function_with_args)
{
    int a = 5, b = 7, got = 0;
    EXPECT_NO_THROW(got = mp::top_catch_all("", std::plus<int>{}, a, b););
    EXPECT_EQ(got, a + b);
}
