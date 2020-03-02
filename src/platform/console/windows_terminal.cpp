/*
 * Copyright (C) 2019-2020 Canonical, Ltd.
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

#include "windows_terminal.h"

#include <iostream>
#include <fcntl.h>
#include <io.h>

namespace mp = multipass;

mp::WindowsTerminal::WindowsTerminal()
    : input_code_page{GetConsoleCP()}, output_code_page{GetConsoleOutputCP()}
{
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);
}

mp::WindowsTerminal::~WindowsTerminal()
{
    SetConsoleCP(input_code_page);
    SetConsoleOutputCP(output_code_page);
}

HANDLE mp::WindowsTerminal::cin_handle() const
{
    return GetStdHandle(STD_INPUT_HANDLE);
}

bool mp::WindowsTerminal::cin_is_live() const
{
    return !(_isatty(_fileno(stdin)) == 0);
}

HANDLE mp::WindowsTerminal::cout_handle() const
{
    return GetStdHandle(STD_OUTPUT_HANDLE);
}

HANDLE mp::WindowsTerminal::cerr_handle() const
{
    return GetStdHandle(STD_ERROR_HANDLE);
}

bool mp::WindowsTerminal::cout_is_live() const
{
    return !(_isatty(_fileno(stdout)) == 0);
}

std::string mp::WindowsTerminal::read_all_cin()
{
    _setmode(_fileno(stdin), _O_BINARY);
    return mp::Terminal::read_all_cin();
}
