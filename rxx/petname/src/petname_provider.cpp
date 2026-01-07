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
 * Authored by: Antoni Bertolin Monferrer <antoni.monferrer@canonical.com>
 *
 */

#include "petname_provider.h"

namespace mp = multipass;

mp::PetnameProvider::PetnameProvider(mp::petname::NumWords num_words, char separator)
    : generator{rxx::petname::make_petname_generator(num_words, separator)}
{
}

std::string mp::PetnameProvider::make_name()
{
    return std::string(generator->make_name());
}
