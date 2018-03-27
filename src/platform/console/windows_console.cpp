/*
 * Copyright (C) 2017-2018 Canonical, Ltd.
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

#include "windows_console.h"

#include <io.h>

namespace mp = multipass;

mp::WindowsConsole::WindowsConsole(ssh_channel channel)
    : interactive{!(_isatty(_fileno(stdin)) == 0)},
      input_handle{GetStdHandle(STD_INPUT_HANDLE)},
      output_handle{GetStdHandle(STD_OUTPUT_HANDLE)},
      saved_geometry{0, 0},
      channel{channel}
{
    events[0] = input_handle;
    events[1] = CreateEvent(NULL, FALSE, FALSE, NULL);
    setup_console();
}

mp::WindowsConsole::~WindowsConsole()
{
    restore_console();
}

void mp::WindowsConsole::setup_console()
{
    if (interactive)
    {
        GetConsoleMode(input_handle, &console_input_mode);
        SetConsoleMode(input_handle,
                       console_input_mode & ~ENABLE_PROCESSED_INPUT & ~ENABLE_LINE_INPUT |
                           ENABLE_VIRTUAL_TERMINAL_INPUT | ENABLE_WINDOW_INPUT);
        GetConsoleMode(output_handle, &console_output_mode);
        SetConsoleMode(output_handle, console_output_mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);

        ssh_channel_request_pty(channel);
        change_ssh_pty_size();
    }
}

int mp::WindowsConsole::read_console(std::array<char, 512>& buffer)
{
    int bytes_read{-1};

    FlushConsoleInputBuffer(input_handle);
    switch (WaitForMultipleObjects(2, events, FALSE, INFINITE))
    {
    case WAIT_OBJECT_0 + 0:
        ReadConsole(input_handle, buffer.data(), buffer.size(), &(DWORD)bytes_read, NULL);
        break;
    case WAIT_OBJECT_0 + 1:
        break;
    }

    return bytes_read;
}

void mp::WindowsConsole::write_console(const char* buffer, int num_bytes)
{
    DWORD write;

    change_ssh_pty_size();
    WriteConsole(output_handle, buffer, num_bytes, &write, NULL);
}

void mp::WindowsConsole::signal_console()
{
    SetEvent(events[1]);
}

// This is needed to see if the console window size has changed. Using WINDOW_BUFFER_SIZE_EVENT from
// ReadConsoleInput() is not enough because the console's buffer size probably won't change when the
// window itself is resized and there is no direct way to get a console window resize event.
void mp::WindowsConsole::change_ssh_pty_size()
{
    CONSOLE_SCREEN_BUFFER_INFO sb_info;
    GetConsoleScreenBufferInfo(output_handle, &sb_info);
    auto columns = sb_info.srWindow.Right - sb_info.srWindow.Left;
    auto rows = sb_info.srWindow.Bottom - sb_info.srWindow.Top + 1;

    if (saved_geometry.columns != columns ||
        saved_geometry.rows != rows)
    {
        saved_geometry.columns = columns;
        saved_geometry.rows = rows;
        ssh_channel_change_pty_size(channel, columns, rows);
    }
}

void mp::WindowsConsole::restore_console()
{
    if (interactive)
    {
        SetConsoleMode(input_handle, console_input_mode);
        SetConsoleMode(output_handle, console_output_mode);
    }
}
