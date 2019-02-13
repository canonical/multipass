/*
 * Copyright (C) 2018-2019 Canonical, Ltd.
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

#ifndef MULTIPASS_OPTIONAL_H
#define MULTIPASS_OPTIONAL_H

#if __has_include(<optional>) && ((__cplusplus > 201402L) || (_MSVC_LANG > 201402L))

#include <optional>
namespace multipass
{
using std::nullopt;
using std::optional;
}

#elif __has_include(<experimental/optional>)

#include <experimental/optional>
namespace multipass
{
using std::experimental::nullopt;
using std::experimental::optional;
}

#else
#error "no optional implementation found!"
#endif

#endif // MULTIPASS_OPTIONAL_H
