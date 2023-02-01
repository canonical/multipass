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

#include "unix_terminal.h"
#include <multipass/terminal.h>
#include <iostream>

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

mp::Terminal::UPtr mp::Terminal::make_terminal()
{
    return std::make_unique<UnixTerminal>();
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

bool mp::Terminal::is_live() const
{
    return cin_is_live() && cout_is_live();
}

std::string mp::Terminal::read_all_cin()
{
    std::string content;
    char arr[1024];
    while (!cin().eof())
    {
        cin().read(arr, sizeof(arr));
        int s = cin().gcount();
        content.append(arr, s);
    }
    return content;
}
