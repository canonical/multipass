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

#include <multipass/exceptions/invalid_memory_size_exception.h>
#include <multipass/memory_size.h>

#include <gtest/gtest.h>

#include <string>

namespace mp = multipass;

using namespace testing;

namespace
{
constexpr auto kilo = 1024LL; // LL: giga times value higher than 4 would overflow if we used only 4bytes here
constexpr auto mega = kilo * kilo;
constexpr auto giga = kilo * mega;
} // namespace

TEST(MemorySize, interpretsKB)
{
    constexpr auto val = 1024;
    const auto size = mp::MemorySize{std::to_string(val) + "KB"};

    EXPECT_EQ(size.in_bytes(), val * kilo);
}

TEST(MemorySize, interpretsK)
{
    constexpr auto val = 1024;
    const auto size = mp::MemorySize{std::to_string(val) + "K"};

    EXPECT_EQ(size.in_bytes(), val * kilo);
}

TEST(MemorySize, interpretsMB)
{
    constexpr auto val = 1024;
    const auto size = mp::MemorySize{std::to_string(val) + "MB"};

    EXPECT_EQ(size.in_bytes(), val * mega);
}

TEST(MemorySize, interpretsM)
{
    constexpr auto val = 1;
    const auto size = mp::MemorySize{std::to_string(val) + "M"};

    EXPECT_EQ(size.in_bytes(), val * mega);
}

TEST(MemorySize, interpretsGB)
{
    constexpr auto val = 1024;
    const auto size = mp::MemorySize{std::to_string(val) + "GB"};

    EXPECT_EQ(size.in_bytes(), val * giga);
}

TEST(MemorySize, interpretsG)
{
    constexpr auto val = 5;
    const auto size = mp::MemorySize{std::to_string(val) + "G"};

    EXPECT_EQ(size.in_bytes(), val * giga);
}

TEST(MemorySize, interpretsNoUnit)
{
    constexpr auto val = 1024;
    const auto size = mp::MemorySize{std::to_string(val)};

    EXPECT_EQ(size.in_bytes(), val);
}

TEST(MemorySize, interpretsB)
{
    constexpr auto val = 123;
    const auto size = mp::MemorySize{std::to_string(val) + "B"};

    EXPECT_EQ(size.in_bytes(), val);
}

TEST(MemorySize, interprets0)
{
    constexpr auto val = 0;
    const auto size = mp::MemorySize{std::to_string(val)};

    EXPECT_EQ(size.in_bytes(), val);
}

TEST(MemorySize, interprets0B)
{
    constexpr auto val = 0;
    const auto size = mp::MemorySize{std::to_string(val) + "B"};

    EXPECT_EQ(size.in_bytes(), val);
}

TEST(MemorySize, interprets0K)
{
    constexpr auto val = 0;
    const auto size = mp::MemorySize{std::to_string(val) + "K"};

    EXPECT_EQ(size.in_bytes(), val);
}

TEST(MemorySize, interprets0M)
{
    constexpr auto val = 0;
    const auto size = mp::MemorySize{std::to_string(val) + "M"};

    EXPECT_EQ(size.in_bytes(), val);
}

TEST(MemorySize, interprets0G)
{
    constexpr auto val = 0;
    const auto size = mp::MemorySize{std::to_string(val) + "G"};

    EXPECT_EQ(size.in_bytes(), val);
}

TEST(MemorySize, converts0ToK)
{
    EXPECT_EQ(mp::MemorySize{"0"}.in_kilobytes(), 0LL);
}

TEST(MemorySize, converts0ToM)
{
    EXPECT_EQ(mp::MemorySize{"0B"}.in_megabytes(), 0LL);
}

TEST(MemorySize, converts0ToG)
{
    EXPECT_EQ(mp::MemorySize{"0G"}.in_gigabytes(), 0LL);
}

TEST(MemorySize, convertsHigherUnitToK)
{
    constexpr auto val = 694;
    EXPECT_EQ(mp::MemorySize{std::to_string(val) + "M"}.in_kilobytes(), val * kilo);
    EXPECT_EQ(mp::MemorySize{std::to_string(val) + "G"}.in_kilobytes(), val * mega);
}

TEST(MemorySize, convertsHigherUnitToM)
{
    constexpr auto val = 653;
    EXPECT_EQ(mp::MemorySize{std::to_string(val) + "G"}.in_megabytes(), val * kilo);
}

TEST(MemorySize, convertsLowerUnitToKWhenExactMultiple)
{
    constexpr auto val = 2;
    EXPECT_EQ(mp::MemorySize{std::to_string(val * kilo)}.in_kilobytes(), val);
}

TEST(MemorySize, convertsLowerUnitToMWhenExactMultiple)
{
    constexpr auto val = 456;
    EXPECT_EQ(mp::MemorySize{std::to_string(val * giga)}.in_megabytes(), val * kilo);
}

TEST(MemorySize, convertsLowerUnitToGWhenExactMultiple)
{
    constexpr auto val = 99;
    EXPECT_EQ(mp::MemorySize{std::to_string(val * giga)}.in_gigabytes(), val);
}

TEST(MemorySize, convertsLowerUnitToKByFlooringWhenNotMultiple)
{
    EXPECT_EQ(mp::MemorySize{"1234B"}.in_kilobytes(), 1);
    EXPECT_EQ(mp::MemorySize{"33B"}.in_kilobytes(), 0);
}

TEST(MemorySize, convertsLowerUnitToMByFlooringWhenNotMultiple)
{
    EXPECT_EQ(mp::MemorySize{"5555K"}.in_megabytes(), 5);
    EXPECT_EQ(mp::MemorySize{"5555B"}.in_megabytes(), 0);
}

TEST(MemorySize, convertsLowerUnitToGByFlooringWhenNotMultiple)
{
    EXPECT_EQ(mp::MemorySize{"2047M"}.in_gigabytes(), 1);
    EXPECT_EQ(mp::MemorySize{"2047K"}.in_gigabytes(), 0);
}

TEST(MemorySize, interpretsSmallcaseUnits)
{
    constexpr auto val = 42;
    EXPECT_EQ(mp::MemorySize{std::to_string(val) + "b"}.in_bytes(), val);
    EXPECT_EQ(mp::MemorySize{std::to_string(val) + "mb"}.in_megabytes(), val);
    EXPECT_EQ(mp::MemorySize{std::to_string(val) + "kB"}.in_kilobytes(), val);
    EXPECT_EQ(mp::MemorySize{std::to_string(val) + "g"}.in_gigabytes(), val);
}

TEST(MemorySize, rejectsBB)
{
    EXPECT_THROW(mp::MemorySize{"321BB"}, mp::InvalidMemorySizeException);
}

TEST(MemorySize, rejectsBK)
{
    EXPECT_THROW(mp::MemorySize{"321BK"}, mp::InvalidMemorySizeException);
}

TEST(MemorySize, rejectsMM)
{
    EXPECT_THROW(mp::MemorySize{"1024MM"}, mp::InvalidMemorySizeException);
}

TEST(MemorySize, rejectsKM)
{
    EXPECT_THROW(mp::MemorySize{"1024KM"}, mp::InvalidMemorySizeException);
}

TEST(MemorySize, rejectsGK)
{
    EXPECT_THROW(mp::MemorySize{"1024GK"}, mp::InvalidMemorySizeException);
}

TEST(MemorySize, rejectsOnlyUnit)
{
    EXPECT_THROW(mp::MemorySize{"K"}, mp::InvalidMemorySizeException);
}

TEST(MemorySize, rejectsEmptyString)
{
    EXPECT_THROW(mp::MemorySize{""}, mp::InvalidMemorySizeException);
}

TEST(MemorySize, rejectsDecimal)
{
    EXPECT_THROW(mp::MemorySize{"123.321K"}, mp::InvalidMemorySizeException);
    EXPECT_THROW(mp::MemorySize{"123.321"}, mp::InvalidMemorySizeException);
}
