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

#include <multipass/petname_interface.h>

#include <regex>
#include <string>
#include <unordered_set>
#include <vector>

namespace mp = multipass;

using namespace testing;

namespace
{
std::vector<std::string> split(const std::string& string, char delimiter)
{
    std::regex regex(std::string(1, delimiter));
    return {std::sregex_token_iterator{string.begin(), string.end(), regex, -1},
            std::sregex_token_iterator{}};
}
} // namespace

// Tests are intended to test the Rust module integration, not the module itself

TEST(PetnameIntegration, usesDefaultSeparator)
{
    char expected_separator{'-'};
    mp::PetnameInterface::UPtr name_generator{multipass::make_petname_provider()};
    auto name = name_generator->make_name();
    auto tokens = split(name, expected_separator);
    EXPECT_THAT(tokens.size(), Eq(2u));
}

TEST(PetnameIntegration, generatesTwoTokensByDefault)
{
    char separator{'-'};
    mp::PetnameInterface::UPtr name_generator{multipass::make_petname_provider(separator)};
    auto name = name_generator->make_name();
    auto tokens = split(name, separator);
    EXPECT_THAT(tokens.size(), Eq(2u));

    // Each token should be unique
    std::unordered_set<std::string> set(tokens.begin(), tokens.end());
    EXPECT_THAT(set.size(), Eq(tokens.size()));
}
