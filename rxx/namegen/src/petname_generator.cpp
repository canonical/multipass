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

#include "petname_generator.h"
#include <namegen/src/lib.rs.h>
#include <random>

namespace mp = multipass;

mp::PetnameGenerator::PetnameGenerator(mp::petname::NumWords num_words, char separator)
    : backend{
          rp::PetnameBackend::make_petname_backend(num_words, separator, std::random_device{}())}
{
}

std::string mp::PetnameGenerator::make_name()
{
    return std::string(backend->make_name());
}
