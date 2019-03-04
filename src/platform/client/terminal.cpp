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

#include <multipass/terminal.h>

#include <iostream>
#include <unistd.h>

namespace mp = multipass;

/* Note on connection between std::cin/cout/cerr and stdin/out/err
 *
 * std::cin/cout/cerr create an i/ostream wrapping around stdin/out/err,
 * but I'm unable to find a cross-platform way to get the FD corresponding
 * to the std library api.
 *
 * Therefore the mix of std C++ and C apis here is an unfortunate but IMO
 * least worst way of doing what we need.
 */
mp::Terminal::~Terminal()
{
}

std::istream& mp::Terminal::cin()
{
    return std::cin;
}

std::ostream& mp::Terminal::cout()
{
    return std::cout;
}

std::ostream& mp::Terminal::cerr()
{
    return std::cerr;
}

int mp::Terminal::cin_fd() const
{
    return fileno(stdin);
}

bool mp::Terminal::cin_is_tty() const
{
    return (isatty(cin_fd()) == 1);
}

int multipass::Terminal::cout_fd() const
{
    return fileno(stdout);
}

bool multipass::Terminal::cout_is_tty() const
{
    return (isatty(cout_fd()) == 1);
}
