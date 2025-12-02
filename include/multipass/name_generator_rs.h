
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

#include <petnamers/src/lib.rs.h>
#include <rust/cxx.h>

// The purpose of this header is to act as 1: a prettyfier of the included header in source and 2:
// to declare all wrappers for the Rust code, which should always include a try catch with rethrow
// plus argument checking with throw if incorrect. Exception type has to be rust::Error because that
// type is declared as final and the caller of the wrapped function will expect that exception but
// could forget about a non-rust::Error-derived exception.

namespace multipass
{
namespace petname
{
std::string generate_petname(petnamers::NumWords word_count, char sep);
}
} // namespace multipass
