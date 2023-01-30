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

#include "mock_libc_functions.h"

#include <stdio.h>
#include <unistd.h>

extern "C"
{
    IMPL_MOCK_DEFAULT(1, getgrnam);

    int ut_isatty(int fd)
    {
        return mock_isatty(fd);
    }

    int ut_fileno(FILE* stream)
    {
        return mock_fileno(stream);
    }

    int ut_tcgetattr(int fd, struct termios* termios_p)
    {
        return mock_tcgetattr(fd, termios_p);
    }

    int ut_tcsetattr(int fd, int optional_actions, const struct termios* termios_p)
    {
        return mock_tcsetattr(fd, optional_actions, termios_p);
    }
}

// By default, call real functions
std::function<int(int)> mock_isatty = isatty;
std::function<int(FILE*)> mock_fileno = fileno;
std::function<int(int, struct termios*)> mock_tcgetattr = tcgetattr;
std::function<int(int, int, const struct termios*)> mock_tcsetattr = tcsetattr;
