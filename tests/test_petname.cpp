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
 * Authored by: Alberto Aguirre <alberto.aguirre@canonical.com>
 *
 */

#include "common.h"

#include <src/petname/petname.h>

#include <regex>
#include <string>
#include <unordered_set>
#include <vector>

namespace mp = multipass;

using namespace testing;

namespace
{
std::vector<std::string> split(const std::string& string, const std::string& delimiter)
{
    std::regex regex(delimiter);
    return {std::sregex_token_iterator{string.begin(), string.end(), regex, -1}, std::sregex_token_iterator{}};
}
}
TEST(Petname, generates_the_requested_num_words)
{
    std::string separator{"-"};
    mp::Petname gen1{mp::Petname::NumWords::ONE, separator};
    mp::Petname gen2{mp::Petname::NumWords::TWO, separator};
    mp::Petname gen3{mp::Petname::NumWords::THREE, separator};

    auto one_word_name = gen1.make_name();
    auto tokens = split(one_word_name, separator);
    EXPECT_THAT(tokens.size(), Eq(1u));

    auto two_word_name = gen2.make_name();
    tokens = split(two_word_name, separator);
    EXPECT_THAT(tokens.size(), Eq(2u));

    auto three_word_name = gen3.make_name();
    tokens = split(three_word_name, separator);
    EXPECT_THAT(tokens.size(), Eq(3u));
}

TEST(Petname, uses_default_separator)
{
    std::string expected_separator{"-"};
    mp::Petname name_generator{mp::Petname::NumWords::THREE};
    auto name = name_generator.make_name();
    auto tokens = split(name, expected_separator);
    EXPECT_THAT(tokens.size(), Eq(3u));
}

TEST(Petname, generates_two_tokens_by_default)
{
    std::string separator{"-"};
    mp::Petname name_generator{separator};
    auto name = name_generator.make_name();
    auto tokens = split(name, separator);
    EXPECT_THAT(tokens.size(), Eq(2u));

    // Each token should be unique
    std::unordered_set<std::string> set(tokens.begin(), tokens.end());
    EXPECT_THAT(set.size(), Eq(tokens.size()));
}

TEST(Petname, can_generate_at_least_hundred_unique_names)
{
    std::string separator{"-"};
    mp::Petname name_generator{mp::Petname::NumWords::THREE, separator};
    std::unordered_set<std::string> name_set;
    const std::size_t expected_num_unique_names{100};

    //TODO: fixme, randomness is involved in name generation hence there's a non-zero probability
    // we will fail to generate the number of expected unique names.
    for (std::size_t i = 0; i < 10*expected_num_unique_names; i++)
    {
        name_set.insert(name_generator.make_name());
    }

    EXPECT_THAT(name_set.size(), Ge(expected_num_unique_names));
}
