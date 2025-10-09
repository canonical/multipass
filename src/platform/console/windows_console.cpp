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

#include "windows_console.h"
#include "windows_terminal.h"

#include <multipass/cli/client_platform.h>
#include <multipass/format.h>
#include <multipass/logging/log.h>

#include <winsock2.h>

#include <array>
#include <string>

namespace mp = multipass;
namespace mpl = multipass::logging;
namespace mcp = multipass::cli::platform;

namespace
{
constexpr auto category = "windows console";
constexpr auto term_type = "xterm-256color";
constexpr auto chunk = 4096u;

mp::Console::ConsoleGeometry get_console_size(HANDLE handle)
{
    CONSOLE_SCREEN_BUFFER_INFO sb_info;
    GetConsoleScreenBufferInfo(handle, &sb_info);
    SHORT columns = sb_info.srWindow.Right - sb_info.srWindow.Left + 1;
    SHORT rows = sb_info.srWindow.Bottom - sb_info.srWindow.Top + 1;

    return {rows, columns}; // rows come before columns in ConsoleGeometry (unlike libssh)
}
} // namespace

mp::WindowsConsole::WindowsConsole(ssh_channel channel, WindowsTerminal* term)
    : interactive{term->cout_is_live()},
      input_handle{term->cin_handle()},
      output_handle{term->cout_handle()},
      error_handle{term->cerr_handle()},
      channel{channel},
      session_socket_fd{ssh_get_fd(ssh_channel_get_session(channel))},
      last_geometry{get_console_size(output_handle)}
{
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

        ssh_channel_request_pty_size(channel, term_type, last_geometry.columns, last_geometry.rows);
    }
}

void mp::WindowsConsole::read_console()
{
    std::array<INPUT_RECORD, chunk> input_records;
    DWORD num_records_read = 0;
    std::basic_string<CHAR> text_buffer;
    text_buffer.reserve(chunk);

    if (!ReadConsoleInput(input_handle, input_records.data(), chunk, &num_records_read))
    {
        mpl::warn(category, "Could not read console input; error code: {}", GetLastError());
    }
    else
    {

        for (auto i = 0u; i < num_records_read; ++i)
        {
            switch (input_records[i].EventType)
            {
            case KEY_EVENT:
            {
                const auto& key_event = input_records[i].Event.KeyEvent;
                if (auto chr = key_event.uChar.AsciiChar;
                    key_event.bKeyDown && chr) // some keys yield null char (e.g. alt)
                    text_buffer.append(key_event.wRepeatCount, chr);
            }
            break;
            case WINDOW_BUFFER_SIZE_EVENT: // The size in this event isn't reliable in WT (see
                                           // microsoft/terminal#281)
                update_ssh_pty_size();     // We obtain it ourselves to update here
                break;
            default:
                break; // ignore
            }
        }

        std::lock_guard<std::mutex> lock(ssh_mutex);
        ssh_channel_write(channel,
                          text_buffer.data(),
                          static_cast<uint32_t>(text_buffer.size() * sizeof(text_buffer.front())));
    }
}

void mp::WindowsConsole::write_console()
{
    DWORD write;
    std::array<char, chunk> buffer;
    int num_bytes{0};
    fd_set read_set;
    HANDLE current_handle{output_handle};

    FD_ZERO(&read_set);
    FD_SET(session_socket_fd, &read_set);

    if (select(0, &read_set, nullptr, nullptr, nullptr) < 1)
        return;

    {
        std::lock_guard<std::mutex> lock(ssh_mutex);
        num_bytes = ssh_channel_read_nonblocking(channel, buffer.data(), chunk, 0);

        // Try reading from stderr if nothing is returned from stdout
        if (num_bytes == 0)
        {
            num_bytes = ssh_channel_read_nonblocking(channel, buffer.data(), chunk, 1);
            current_handle = error_handle;
        }
    }

    if (num_bytes < 1)
    {
        // Force the channel to close if EOF is detected from the channel_read
        if (num_bytes == SSH_EOF)
            ssh_channel_close(channel);

        return;
    }

    DWORD mode;
    if (GetConsoleMode(current_handle, &mode))
    {
        WriteConsole(current_handle, buffer.data(), num_bytes, &write, nullptr);
    }
    else
    {
        WriteFile(current_handle, buffer.data(), num_bytes, &write, nullptr);
    }
}

void mp::WindowsConsole::exit_console()
{
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

void mp::WindowsConsole::update_ssh_pty_size()
{
    auto [rows, columns] = get_console_size(output_handle);

    if (last_geometry.columns != columns || last_geometry.rows != rows)
    {
        last_geometry.columns = columns;
        last_geometry.rows = rows;
        std::lock_guard<std::mutex> lock(ssh_mutex);
        ssh_channel_change_pty_size(channel, columns, rows);
    }
}
