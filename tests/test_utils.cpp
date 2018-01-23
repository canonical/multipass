/*
 * Copyright (C) 2017-2018 Canonical, Ltd.
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

#include <multipass/utils.h>

#include <gtest/gtest.h>

namespace mp = multipass;
using namespace testing;

TEST(Utils, KB_is_valid)
{
    EXPECT_TRUE(mp::utils::valid_memory_value(QString("1024KB")));
}

TEST(Utils, K_is_valid)
{
    EXPECT_TRUE(mp::utils::valid_memory_value(QString("1024K")));
}

TEST(Utils, MB_is_valid)
{
    EXPECT_TRUE(mp::utils::valid_memory_value(QString("1024MB")));
}

TEST(Utils, M_is_valid)
{
    EXPECT_TRUE(mp::utils::valid_memory_value(QString("1024M")));
}

TEST(Utils, GB_is_valid)
{
    EXPECT_TRUE(mp::utils::valid_memory_value(QString("1024GB")));
}

TEST(Utils, G_is_valid)
{
    EXPECT_TRUE(mp::utils::valid_memory_value(QString("1024G")));
}

TEST(Utils, no_unit_is_valid)
{
    EXPECT_TRUE(mp::utils::valid_memory_value(QString("1024")));
}

TEST(Utils, MM_unit_is_invalid)
{
    EXPECT_FALSE(mp::utils::valid_memory_value(QString("1024MM")));
}

TEST(Utils, hostname_begins_with_letter_is_valid)
{
    EXPECT_TRUE(mp::utils::valid_hostname(QString("foo")));
}

TEST(Utils, hostname_single_letter_is_valid)
{
    EXPECT_TRUE(mp::utils::valid_hostname(QString("f")));
}

TEST(Utils, hostname_contains_digit_is_valid)
{
    EXPECT_TRUE(mp::utils::valid_hostname(QString("foo1")));
}

TEST(Utils, hostname_contains_hyphen_is_valid)
{
    EXPECT_TRUE(mp::utils::valid_hostname(QString("foo-bar")));
}

TEST(Utils, hostname_begins_with_digit_is_invalid)
{
    EXPECT_FALSE(mp::utils::valid_hostname(QString("1foo")));
}

TEST(Utils, hostname_single_digit_is_invalid)
{
    EXPECT_FALSE(mp::utils::valid_hostname(QString("1")));
}

TEST(Utils, hostname_contains_underscore_is_invalid)
{
    EXPECT_FALSE(mp::utils::valid_hostname(QString("foo_bar")));
}

TEST(Utils, hostname_contains_special_character_is_invalid)
{
    EXPECT_FALSE(mp::utils::valid_hostname(QString("foo!")));
}

TEST(Utils, path_root_invalid)
{
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("/")));
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("//")));
}

TEST(Utils, path_root_foo_valid)
{
    EXPECT_FALSE(mp::utils::invalid_target_path(QString("/foo")));
    EXPECT_FALSE(mp::utils::invalid_target_path(QString("/foo/")));
    EXPECT_FALSE(mp::utils::invalid_target_path(QString("//foo")));
}

TEST(Utils, path_dev_invalid)
{
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("/dev")));
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("/dev/")));
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("//dev/")));
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("/dev//")));
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("//dev//")));
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("/dev/foo")));
}

TEST(Utils, path_devpath_valid)
{
    EXPECT_FALSE(mp::utils::invalid_target_path(QString("/devpath")));
    EXPECT_FALSE(mp::utils::invalid_target_path(QString("/devpath/")));
    EXPECT_FALSE(mp::utils::invalid_target_path(QString("/devpath/foo")));
}

TEST(Utils, path_proc_invalid)
{
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("/proc")));
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("/proc/")));
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("//proc/")));
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("/proc//")));
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("//proc//")));
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("/proc/foo")));
}

TEST(Utils, path_sys_invalid)
{
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("/sys")));
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("/sys/")));
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("//sys/")));
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("/sys//")));
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("//sys//")));
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("/sys/foo")));
}

TEST(Utils, path_home_ubuntu_invalid)
{
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("/home/ubuntu")));
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("/home/ubuntu/")));
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("//home/ubuntu/")));
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("/home//ubuntu/")));
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("/home/ubuntu//")));
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("//home//ubuntu//")));
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("/home/ubuntu/foo/..")));
}

TEST(Utils, path_home_ubuntu_foo_valid)
{
    EXPECT_FALSE(mp::utils::invalid_target_path(QString("/home/ubuntu/foo")));
    EXPECT_FALSE(mp::utils::invalid_target_path(QString("/home/ubuntu/foo/")));
    EXPECT_FALSE(mp::utils::invalid_target_path(QString("//home/ubuntu/foo")));
}
