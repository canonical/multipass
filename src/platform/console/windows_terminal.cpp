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

#include "windows_terminal.h"

#include "windows_console.h"

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
    // GetConsoleMode will fail if cin is not a console. If it does not fail, we check if cin can receive input.
    DWORD mode;
    return GetConsoleMode(cin_handle(), &mode) && (mode & ENABLE_LINE_INPUT);
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
    // GetConsoleScreenBufferInfo will fail if cout is not a console. Since there is nothing to check in the
    // CONSOLE_SCREEN_BUFFER_INFO structure, we return the result of GetConsoleScreenBufferInfo.
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    return GetConsoleScreenBufferInfo(cout_handle(), &csbi);
}

std::string mp::WindowsTerminal::read_all_cin()
{
    _setmode(_fileno(stdin), _O_BINARY);
    return mp::Terminal::read_all_cin();
}

void mp::WindowsTerminal::set_cin_echo(const bool enable)
{
    DWORD console_input_mode;

    GetConsoleMode(cin_handle(), &console_input_mode);

    if (!enable)
        console_input_mode &= ~ENABLE_ECHO_INPUT;
    else
        console_input_mode |= ENABLE_ECHO_INPUT;

    SetConsoleMode(cin_handle(), console_input_mode);
}

mp::WindowsTerminal::ConsolePtr mp::WindowsTerminal::make_console(ssh_channel channel)
{
    return std::make_unique<WindowsConsole>(channel, this);
}
