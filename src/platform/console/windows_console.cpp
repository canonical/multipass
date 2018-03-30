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

#include <array>
#include <mutex>

#include <io.h>

#include <iostream>
namespace mp = multipass;

namespace
{
// Needed so the WinEventProc callback can use it
ssh_channel global_channel;
HANDLE global_output_handle;
mp::Console::ConsoleGeometry last_geometry{0, 0};
std::mutex ssh_mutex;

void change_ssh_pty_size(const HANDLE output_handle)
{
    CONSOLE_SCREEN_BUFFER_INFO sb_info;
    GetConsoleScreenBufferInfo(output_handle, &sb_info);
    auto columns = sb_info.srWindow.Right - sb_info.srWindow.Left;
    auto rows = sb_info.srWindow.Bottom - sb_info.srWindow.Top + 1;

    if (last_geometry.columns != columns || last_geometry.rows != rows)
    {
        last_geometry.columns = columns;
        last_geometry.rows = rows;
        std::lock_guard<std::mutex> lock(ssh_mutex);
        ssh_channel_change_pty_size(global_channel, columns, rows);
    }
}

void CALLBACK handle_console_resize(HWINEVENTHOOK hook, DWORD event, HWND hwnd, LONG idObject, LONG idChild,
                                    DWORD dwEventThread, DWORD dwmsEventTime)
{
    if (hwnd == GetConsoleWindow())
    {
        change_ssh_pty_size(global_output_handle);
    }
}

void monitor_console_resize(HWINEVENTHOOK& hook)
{
    MSG msg;
    BOOL ret;

    hook = SetWinEventHook(EVENT_CONSOLE_LAYOUT, EVENT_CONSOLE_LAYOUT, nullptr, &handle_console_resize, 0, 0,
                           WINEVENT_OUTOFCONTEXT);

    while ((ret = GetMessage(&msg, nullptr, 0, 0)) != 0)
    {
        if (ret == -1)
            break;

        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}
} // namespace

mp::WindowsConsole::WindowsConsole(ssh_channel channel)
    : interactive{!(_isatty(_fileno(stdin)) == 0)},
      input_handle{GetStdHandle(STD_INPUT_HANDLE)},
      output_handle{GetStdHandle(STD_OUTPUT_HANDLE)},
      channel{channel},
      console_event_thread{[this] { monitor_console_resize(hook); }}
{
    global_channel = channel;
    global_output_handle = output_handle;
    setup_console();
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
        change_ssh_pty_size(output_handle);
    }
}

void mp::WindowsConsole::read_console()
{
    std::array<char, 1024> buffer;
    int bytes_read{0};

    FlushConsoleInputBuffer(input_handle);
    ReadConsole(input_handle, buffer.data(), buffer.size(), &(DWORD)bytes_read, NULL);

    std::lock_guard<std::mutex> lock(ssh_mutex);
    ssh_channel_write(channel, buffer.data(), bytes_read);
}

void mp::WindowsConsole::write_console()
{
    DWORD write;
    std::array<char, 1024> buffer;
    int num_bytes{0};

    if (ssh_channel_poll(channel, 0) < 1)
        return;

    {
        std::lock_guard<std::mutex> lock(ssh_mutex);
        num_bytes = ssh_channel_read_nonblocking(channel, buffer.data(), buffer.size(), 0);
    }

    WriteConsole(output_handle, buffer.data(), num_bytes, &write, NULL);
}

void mp::WindowsConsole::exit_console()
{
    PostThreadMessage(GetThreadId(console_event_thread.native_handle()), WM_QUIT, 0, 0);
    UnhookWinEvent(hook);

    if (console_event_thread.joinable())
        console_event_thread.join();

    restore_console();
    FreeConsole();
}

void mp::WindowsConsole::restore_console()
{
    if (interactive)
    {
        SetConsoleMode(input_handle, console_input_mode);
        SetConsoleMode(output_handle, console_output_mode);
    }
}
