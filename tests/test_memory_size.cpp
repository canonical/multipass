
/*
 * Copyright (C) 2019-2022 Canonical, Ltd.
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

#include <multipass/exceptions/invalid_memory_size_exception.h>
#include <multipass/memory_size.h>

#include <string>
#include <tuple>
#include <vector>

namespace mp = multipass;

using namespace testing;

namespace
{
constexpr auto kilo = 1024LL; // LL: giga times value higher than 4 would overflow if we used only 4bytes here
constexpr auto mega = kilo * kilo;
constexpr auto giga = kilo * mega;

struct TestGoodMemorySizeFormats : public TestWithParam<std::tuple<long long, std::string, long long>>
{
    struct UnitSpec
    {
        std::vector<std::string> suffixes;
        long long factor;

        auto gen_unit_args() const
        {
            std::vector<std::tuple<std::string, long long>> ret;
            for(const auto& suffix : suffixes)
                ret.emplace_back(suffix, factor);

            return ret;
        }
    };

    static auto generate_unit_args()
    {
        const UnitSpec byte_unit{{"", "b", "B"}, 1LL};
        const UnitSpec kilo_unit{{"k", "kb", "kB", "Kb", "KB", "K", "KiB"}, kilo};
        const UnitSpec mega_unit{{"m", "mb", "mB", "Mb", "MB", "M", "MiB"}, mega};
        const UnitSpec giga_unit{{"g", "gb", "gB", "Gb", "GB", "G", "GiB"}, giga};

        auto args = byte_unit.gen_unit_args();
        const auto kilo_args = kilo_unit.gen_unit_args();
        const auto mega_args = mega_unit.gen_unit_args();
        const auto giga_args = giga_unit.gen_unit_args();

        args.insert(std::end(args), std::cbegin(kilo_args), std::cend(kilo_args));
        args.insert(std::end(args), std::cbegin(mega_args), std::cend(mega_args));
        args.insert(std::end(args), std::cbegin(giga_args), std::cend(giga_args));

        return args;
    }

    static auto generate_args()
    {
        const auto values = {0LL, 1LL, 42LL, 1023LL, 1024LL, 2048LL, 2049LL};
        const auto unit_args = generate_unit_args();
        std::vector<std::tuple<long long, std::string, long long>> ret;

        for(auto val : values)
            for(const auto& uarg : unit_args)
                ret.emplace_back(val, get<0>(uarg), get<1>(uarg));

        return ret;
    }
};

struct TestBadMemorySizeFormats : public TestWithParam<std::string>
{
};

} // namespace

TEST_P(TestGoodMemorySizeFormats, interpretsValidFormats)
{
    auto param = GetParam();
    const auto val = get<0>(param);
    const auto unit = get<1>(param);
    const auto factor = get<2>(param);

    const auto size = mp::MemorySize{std::to_string(val) + unit};
    EXPECT_EQ(size.in_bytes(), val * factor);
}

TEST_P(TestBadMemorySizeFormats, rejectsBadFormats)
{
    EXPECT_THROW(mp::MemorySize{GetParam()}, mp::InvalidMemorySizeException);
}

INSTANTIATE_TEST_SUITE_P(MemorySize, TestGoodMemorySizeFormats, ValuesIn(TestGoodMemorySizeFormats::generate_args()));
INSTANTIATE_TEST_SUITE_P(MemorySize, TestBadMemorySizeFormats, Values("321BB", "321BK", "1024MM", "1024KM", "1024GK", "K", "", "123.321K", "123.321"));

TEST(MemorySize, defaultConstructsToZero)
{
    EXPECT_EQ(mp::MemorySize{}.in_bytes(), 0LL);
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

TEST(MemorySize, canCompareEqual)
{
    mp::MemorySize x{"999"};
    EXPECT_EQ(x, x);
    EXPECT_EQ(x, mp::MemorySize{x});
    EXPECT_EQ(mp::MemorySize{}, mp::MemorySize{"0B"});
    EXPECT_EQ(mp::MemorySize{"2048"}, mp::MemorySize{"2k"});
    EXPECT_EQ(mp::MemorySize{"2g"}, mp::MemorySize{"2048M"});
    EXPECT_EQ(mp::MemorySize{"2g"}, mp::MemorySize{"2048M"});
    EXPECT_EQ(mp::MemorySize{"0m"}, mp::MemorySize{"0k"});
}

TEST(MemorySize, canCompareNotEqual)
{
    EXPECT_NE(mp::MemorySize{"2048b"}, mp::MemorySize{"2g"});
    EXPECT_NE(mp::MemorySize{"42g"}, mp::MemorySize{"42m"});
    EXPECT_NE(mp::MemorySize{"123"}, mp::MemorySize{"321"});
    EXPECT_NE(mp::MemorySize{"2352346"}, mp::MemorySize{"0"});
}

TEST(MemorySize, canCompareGreater)
{
    EXPECT_GT(mp::MemorySize{"2048b"}, mp::MemorySize{"2"});
    EXPECT_GT(mp::MemorySize{"42g"}, mp::MemorySize{"42m"});
    EXPECT_GT(mp::MemorySize{"1234"}, mp::MemorySize{"321"});
    EXPECT_GT(mp::MemorySize{"2352346"}, mp::MemorySize{"0"});
}

TEST(MemorySize, canCompareGreaterEqual)
{
    EXPECT_GE(mp::MemorySize{"2048b"}, mp::MemorySize{"2"});
    EXPECT_GE(mp::MemorySize{"0m"}, mp::MemorySize{"0k"});
    EXPECT_GE(mp::MemorySize{"76"}, mp::MemorySize{"76"});
    EXPECT_GE(mp::MemorySize{"7k"}, mp::MemorySize{"6k"});
}

TEST(MemorySize, canCompareLess)
{
    EXPECT_LT(mp::MemorySize{"2047b"}, mp::MemorySize{"2k"});
    EXPECT_LT(mp::MemorySize{"42g"}, mp::MemorySize{"420g"});
    EXPECT_LT(mp::MemorySize{"123"}, mp::MemorySize{"321"});
    EXPECT_LT(mp::MemorySize{"2352346"}, mp::MemorySize{"55g"});
}

TEST(MemorySize, canCompareLessEqual)
{
    EXPECT_LE(mp::MemorySize{"2"}, mp::MemorySize{"2048b"});
    EXPECT_LE(mp::MemorySize{"0k"}, mp::MemorySize{"0m"});
    EXPECT_LE(mp::MemorySize{"76"}, mp::MemorySize{"76"});
    EXPECT_LE(mp::MemorySize{"6k"}, mp::MemorySize{"7k"});
}

using mem_repr = std::tuple<mp::MemorySize, std::string>;
struct TestHumanReadableSizes : public TestWithParam<mem_repr>
{
};

TEST_P(TestHumanReadableSizes, producesProperHumanReadableFormat)
{
    const auto& [size, repr] = GetParam();
    EXPECT_EQ(size.human_readable(), repr);
}

INSTANTIATE_TEST_SUITE_P(MemorySize, TestHumanReadableSizes,
                         Values(mem_repr{"0", "0B"}, mem_repr{"42B", "42B"}, mem_repr{"50B", "50B"},
                                mem_repr{"999", "999B"}, mem_repr{"1023", "1023B"}, mem_repr{"1024", "1.0KiB"},
                                mem_repr{"1031", "1.0KiB"}, mem_repr{"999K", "999.0KiB"}, mem_repr{"4096K", "4.0MiB"},
                                mem_repr{"4546K", "4.4MiB"}, mem_repr{"8653K", "8.5MiB"}, mem_repr{"9999M", "9.8GiB"},
                                mem_repr{"1234567890", "1.1GiB"}, mem_repr{"123456G", "123456.0GiB"}));
