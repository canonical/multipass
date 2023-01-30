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

#include "petname.h"
#include "multipass/petname/names.h"

#include <iostream>

namespace mp = multipass;
namespace
{
constexpr auto num_names = std::extent<decltype(mp::petname::names)>::value;
constexpr auto num_adverbs = std::extent<decltype(mp::petname::adverbs)>::value;
constexpr auto num_adjectives = std::extent<decltype(mp::petname::adjectives)>::value;

// Arbitrary but arrays should have at least 100 entries each
static_assert(num_names >= 100, "");
static_assert(num_adverbs >= 100, "");
static_assert(num_adjectives >= 100, "");

std::mt19937 make_engine()
{
    std::random_device device;
    return std::mt19937(device());
}
}

mp::Petname::Petname(std::string separator)
    : Petname(NumWords::TWO, separator)
{
}

mp::Petname::Petname(NumWords num_words)
    : Petname(num_words, "-")
{
}

mp::Petname::Petname(NumWords num_words, std::string separator)
    : separator{separator},
      num_words{num_words},
      engine{make_engine()},
      name_dist{1, num_names - 1},
      adjective_dist{0, num_adjectives - 1},
      adverb_dist{0, num_adverbs - 1}
{
}

std::string mp::Petname::make_name()
{
    std::string name = multipass::petname::names[name_dist(engine)];
    std::string adjective = multipass::petname::adjectives[adjective_dist(engine)];
    std::string adverb = multipass::petname::adverbs[adverb_dist(engine)];

    switch(num_words)
    {
    case NumWords::ONE:
        return name;
    case NumWords::TWO:
        return adjective + separator + name;
    case NumWords::THREE:
        return adverb + separator + adjective + separator + name;
    default:
        throw std::invalid_argument("Invalid number of words chosen");
    }
}
