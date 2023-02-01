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

#ifndef MULTIPASS_MOCK_LIBC_FUNCTIONS_H
#define MULTIPASS_MOCK_LIBC_FUNCTIONS_H

#include <premock.hpp>

#include <grp.h>
#include <stdio.h>
#include <termios.h>

DECL_MOCK(getgrnam);

// The following mocks can't use premock directly because they are declared noexcept and the way
// premock works is that it uses decltype() to get the function types and then uses this when declaring
// the std::function.  However, in C++17, std::function can no longer be declared with a noexcept type, so
// to avoid that, we basically do what the premock macros do here but be explicit about the types and
// not use noexcept.
extern "C" std::function<int(int)> mock_isatty;
extern "C" std::function<int(FILE*)> mock_fileno;
extern "C" std::function<int(int, struct termios*)> mock_tcgetattr;
extern "C" std::function<int(int, int, const struct termios*)> mock_tcsetattr;
#endif // MULTIPASS_MOCK_LIBC_FUNCTIONS_H
