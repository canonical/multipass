/*
 * Copyright (C) 2019 Canonical, Ltd.
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

#include "unix_terminal.h"

#include <iostream>
#include <unistd.h>

namespace mp = multipass;


int mp::UnixTerminal::cin_fd() const
{
    return fileno(stdin);
}

bool mp::UnixTerminal::cin_is_live() const
{
    return (isatty(cin_fd()) == 1);
}

int multipass::UnixTerminal::cout_fd() const
{
    return fileno(stdout);
}

bool multipass::UnixTerminal::cout_is_live() const
{
    return (isatty(cout_fd()) == 1);
}
