/*
 * Copyright (C) 2019 Canonical, Ltd.
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

#include "src/platform/update/version.h"

#include <gmock/gmock.h>

namespace mp = multipass;
using namespace testing;

TEST(Version, simple_tag)
{
    mp::Version v("v3.14");
    EXPECT_EQ(3, v.major());
    EXPECT_EQ(14, v.minor());
    EXPECT_EQ("", v.modifier());
}

TEST(Version, more_complex_tag)
{
    mp::Version v("v3.14-pre3");
    EXPECT_EQ(3, v.major());
    EXPECT_EQ(14, v.minor());
    EXPECT_EQ("pre3", v.modifier());
}

TEST(Version, git_describe_long)
{
    mp::Version v("v0.1-124-ge428aah");
    EXPECT_EQ(0, v.major());
    EXPECT_EQ(1, v.minor());
    EXPECT_EQ("", v.modifier());
}

TEST(Version, git_describe_longer)
{
    mp::Version v("v1.12314-full-9-ge428aah");
    EXPECT_EQ(1, v.major());
    EXPECT_EQ(12314, v.minor());
    EXPECT_EQ("full", v.modifier());
}

TEST(Version, bad_tag_throws1)
{
    EXPECT_THROW(mp::Version v("3.14"), std::runtime_error);
}

TEST(Version, bad_tag_throws2)
{
    EXPECT_THROW(mp::Version v("2018.12.1-rc2"), std::runtime_error);
}

TEST(Version, bad_tag_throws3)
{
    EXPECT_THROW(mp::Version v("a.b"), std::runtime_error);
}

TEST(Version, bad_tag_throws4)
{
    EXPECT_THROW(mp::Version v("va.b"), std::runtime_error);
}

TEST(Version, bad_tag_throws5)
{
    EXPECT_THROW(mp::Version v("v5"), std::runtime_error);
}

TEST(Version, compare_simple)
{
    mp::Version v1("v3.14"), v2("v4.0");
    EXPECT_TRUE(v1 < v2);
}

TEST(Version, compare_simple2)
{
    mp::Version v1("v3.14"), v2("v3.15");
    EXPECT_TRUE(v1 < v2);
}

TEST(Version, compare_complex)
{
    mp::Version v1("v0.01-full"), v2("v1.0002-something");
    EXPECT_TRUE(v1 < v2);
}

TEST(Version, compare_equal_simple)
{
    mp::Version v1("v41.2"), v2("v41.2");
    EXPECT_FALSE(v1 < v2);
}

TEST(Version, compare_equal_but_with_pre_modifier)
{
    mp::Version v1("v1.23-pre5"), v2("v1.23");
    EXPECT_TRUE(v1 < v2);
}

TEST(Version, compare_different_with_pre_modifier)
{
    mp::Version v1("v1.24-pre1"), v2("v1.23-full");
    EXPECT_FALSE(v1 < v2);
}

TEST(Version, compare_equal_but_with_pre_modifier2)
{
    mp::Version v1("v1.23"), v2("v1.23-pre1");
    EXPECT_FALSE(v1 < v2);
}

TEST(Version, compare_equal_with_pre)
{
    mp::Version v1("v1.23-pre1"), v2("v1.23-pre1");
    EXPECT_FALSE(v1 < v2);
}
