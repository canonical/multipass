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

#include <src/rustipass/rust_petname_generator.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <tests/common.h>

namespace mpt = multipass::test;

using namespace testing;

struct RustPetnameGeneratorTests : public Test
{
};

// Test valid constructor with 1 word
TEST_F(RustPetnameGeneratorTests, constructorWithOneWord)
{
    EXPECT_NO_THROW({ multipass::RustPetnameGenerator generator(1, "-"); });
}

// Test valid constructor with 2 words (default)
TEST_F(RustPetnameGeneratorTests, constructorWithTwoWords)
{
    EXPECT_NO_THROW({ multipass::RustPetnameGenerator generator(2, "-"); });
}

// Test valid constructor with 3 words
TEST_F(RustPetnameGeneratorTests, constructorWithThreeWords)
{
    EXPECT_NO_THROW({ multipass::RustPetnameGenerator generator(3, "-"); });
}

// Test default constructor
TEST_F(RustPetnameGeneratorTests, constructorWithDefaults)
{
    EXPECT_NO_THROW({ multipass::RustPetnameGenerator generator; });
}

// Test invalid constructor with 0 words - should throw
TEST_F(RustPetnameGeneratorTests, constructorWithZeroWordsThrows)
{
    MP_EXPECT_THROW_THAT(multipass::RustPetnameGenerator generator(0, "-"),
                         std::runtime_error,
                         mpt::match_what(AllOf(HasSubstr("Failed to create petname generator"),
                                               HasSubstr("Invalid number of words: 0"))));
}

// Test invalid constructor with 4 words - should throw
TEST_F(RustPetnameGeneratorTests, constructorWithFourWordsThrows)
{
    MP_EXPECT_THROW_THAT(multipass::RustPetnameGenerator generator(4, "-"),
                         std::runtime_error,
                         mpt::match_what(AllOf(HasSubstr("Failed to create petname generator"),
                                               HasSubstr("Invalid number of words: 4"))));
}

// Test invalid constructor with negative words - should throw
TEST_F(RustPetnameGeneratorTests, constructorWithNegativeWordsThrows)
{
    MP_EXPECT_THROW_THAT(multipass::RustPetnameGenerator generator(-1, "-"),
                         std::runtime_error,
                         mpt::match_what(AllOf(HasSubstr("Failed to create petname generator"),
                                               HasSubstr("Invalid number of words: -1"))));
}

// Test invalid constructor with large number - should throw
TEST_F(RustPetnameGeneratorTests, constructorWithLargeNumberThrows)
{
    MP_EXPECT_THROW_THAT(multipass::RustPetnameGenerator generator(100, "-"),
                         std::runtime_error,
                         mpt::match_what(AllOf(HasSubstr("Failed to create petname generator"),
                                               HasSubstr("Invalid number of words: 100"))));
}

// Test make_name returns non-empty string with 1 word
TEST_F(RustPetnameGeneratorTests, makeNameOneWordReturnsNonEmpty)
{
    multipass::RustPetnameGenerator generator(1, "-");
    auto name = generator.make_name();
    EXPECT_FALSE(name.empty());
    EXPECT_THAT(name, Not(HasSubstr("-"))); // Single word should not have separator
}

// Test make_name returns non-empty string with 2 words
TEST_F(RustPetnameGeneratorTests, makeNameTwoWordsReturnsNonEmpty)
{
    multipass::RustPetnameGenerator generator(2, "-");
    auto name = generator.make_name();
    EXPECT_FALSE(name.empty());
    EXPECT_THAT(name, HasSubstr("-")); // Two words should have separator
}

// Test make_name returns non-empty string with 3 words
TEST_F(RustPetnameGeneratorTests, makeNameThreeWordsReturnsNonEmpty)
{
    multipass::RustPetnameGenerator generator(3, "-");
    auto name = generator.make_name();
    EXPECT_FALSE(name.empty());
    EXPECT_THAT(name, HasSubstr("-")); // Three words should have separator
}

// Test custom separator works
TEST_F(RustPetnameGeneratorTests, customSeparatorWorks)
{
    multipass::RustPetnameGenerator generator(2, "_");
    auto name = generator.make_name();
    EXPECT_FALSE(name.empty());
    EXPECT_THAT(name, HasSubstr("_"));
    EXPECT_THAT(name, Not(HasSubstr("-")));
}

// Test empty separator works
TEST_F(RustPetnameGeneratorTests, emptySeparatorWorks)
{
    multipass::RustPetnameGenerator generator(2, "");
    auto name = generator.make_name();
    EXPECT_FALSE(name.empty());
    EXPECT_THAT(name, Not(HasSubstr("-")));
    EXPECT_THAT(name, Not(HasSubstr("_")));
}

// Test make_name can be called multiple times
TEST_F(RustPetnameGeneratorTests, makeNameCanBeCalledMultipleTimes)
{
    multipass::RustPetnameGenerator generator(2, "-");

    auto name1 = generator.make_name();
    auto name2 = generator.make_name();
    auto name3 = generator.make_name();

    EXPECT_FALSE(name1.empty());
    EXPECT_FALSE(name2.empty());
    EXPECT_FALSE(name3.empty());

    // Names should be random, but we can't guarantee they're different
    // Just verify they're all valid
    EXPECT_THAT(name1, HasSubstr("-"));
    EXPECT_THAT(name2, HasSubstr("-"));
    EXPECT_THAT(name3, HasSubstr("-"));
}

// Test that make_name with default constructor works
TEST_F(RustPetnameGeneratorTests, makeNameWithDefaultConstructorWorks)
{
    multipass::RustPetnameGenerator generator;
    auto name = generator.make_name();
    EXPECT_FALSE(name.empty());
    EXPECT_THAT(name, HasSubstr("-")); // Default is 2 words with "-" separator
}
