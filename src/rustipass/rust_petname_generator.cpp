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

#include "rust_petname_generator.h"

#include <stdexcept>

namespace multipass
{

RustPetnameGenerator::RustPetnameGenerator(int num_words, const std::string& separator)
    : petname_generator([&]() {
          try
          {
              return multipass::petname::new_petname(num_words, separator.c_str());
          }
          catch (const rust::Error& e)
          {
              throw std::runtime_error(std::string("Failed to create petname generator: ") +
                                       e.what());
          }
      }())
{
}

std::string RustPetnameGenerator::make_name()
{
    try
    {
        return std::string(multipass::petname::make_name(*petname_generator));
    }
    catch (const rust::Error& e)
    {
        throw std::runtime_error(std::string("Failed to generate petname: ") + e.what());
    }
}

} // namespace multipass
