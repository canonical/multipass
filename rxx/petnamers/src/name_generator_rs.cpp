
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

#include "petnamers/src/lib.rs.h"
#include <multipass/name_generator_rs.h>

namespace mpp = multipass::petname;
namespace mp = multipass;

std::string mpp::generate_petname(petnamers::NumWords word_count, char sep)
{
    if (word_count >= petnamers::NumWords::Max || word_count < petnamers::NumWords::One)
        return "";
    try
    {
        return std::string(petnamers::generate_petname(word_count, sep));
    }
    catch (const rust::Error& e)
    {
        std::string error{e.what()};
        if (error.find("some_error"))
        {
            throw;
        }
        else
        {
            throw;
        }
    }
}
