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

#pragma once

#include <multipass/name_generator.h>

#include <rust/cxx.h>

// Forward declare the Rust type
namespace multipass::petname
{
struct Petname;
rust::Box<Petname> new_petname(int32_t num_words, rust::Str separator);
rust::String make_name(Petname& petname);
} // namespace multipass::petname

namespace multipass
{

class RustPetnameGenerator : public NameGenerator
{
public:
    explicit RustPetnameGenerator(int num_words = 2, const std::string& separator = "-");

    std::string make_name() override;

private:
    rust::Box<multipass::petname::Petname> petname_generator;
};

} // namespace multipass
