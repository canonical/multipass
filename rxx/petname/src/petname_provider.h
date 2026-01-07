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

#pragma once

#include <multipass/petname_interface.h>

#include <petname/src/lib.rs.h>
#include <rust/cxx.h>

#include <string>

namespace multipass
{
class PetnameProvider final : public PetnameInterface
{
public:
    /// Constructs an instance that will generate names using
    /// the requested separator and the requested number of words
    PetnameProvider(petname::NumWords num_words, char separator);
    /// Constructs an instance that will generate names using
    /// a default separator of "-" and the requested number of words
    explicit PetnameProvider(petname::NumWords num_words);
    /// Constructs an instance that will generate names using
    /// the requested separator and two words
    explicit PetnameProvider(char separator);

    std::string make_name() override;

private:
    rust::Box<rxx::petname::PetnameGenerator> generator;
};
} // namespace multipass
