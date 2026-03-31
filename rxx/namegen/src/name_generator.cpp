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

#include "petname_generator.h"

#include <multipass/name_generator.h>

#include <memory>

namespace mp = multipass;
namespace mpp = multipass::petname;

mp::NameGenerator::UPtr mpp::detail::make_petname_provider_impl(mpp::NumWords num_words,
                                                                char separator)
{
    return std::make_unique<PetnameGenerator>(num_words, separator);
}
