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

#ifndef MULTIPASS_PETNAME_H
#define MULTIPASS_PETNAME_H

#include <multipass/name_generator.h>

#include <random>
#include <string>
#include <vector>

namespace multipass
{
class Petname final : public NameGenerator
{
public:
    enum class NumWords
    {
        ONE,
        TWO,
        THREE
    };

    /// Constructs an instance that will generate names using
    /// the requested separator and the requested number of words
    Petname(NumWords num_words, std::string separator);
    /// Constructs an instance that will generate names using
    /// a default separator of "-" and the requested number of words
    explicit Petname(NumWords num_words);
    /// Constructs an instance that will generate names using
    /// the requested separator and two words
    explicit Petname(std::string separator);

    std::string make_name() override;

private:
    std::string separator;
    NumWords num_words;
    std::mt19937 engine;
    std::uniform_int_distribution<std::size_t> name_dist;
    std::uniform_int_distribution<std::size_t> adjective_dist;
    std::uniform_int_distribution<std::size_t> adverb_dist;
};
}
#endif // MULTIPASS_PETNAME_H
