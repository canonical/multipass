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

#include <multipass/format.h>
#include <multipass/logging/log.h>
#include <multipass/ssh/libssh_wrapper.h>
#include <multipass/ssh/platform_ssh_client.h>

#include <array>
#include <mutex>

#include <csignal>
#include <io.h>
#include <thread>

#include <winsock2.h>

namespace mp = multipass;
namespace mpl = multipass::logging;

struct mp::PlatformSSHClient::PlatformData
{
    mp::NativeHandles handles;
    DWORD console_input_mode;
    DWORD console_output_mode;
    ssh_channel channel;
    socket_t session_socket_fd;
    ConsoleGeometry last_geometry;
    mutable std::mutex ssh_mutex;

    PlatformData(mp::NativeHandles handles, ssh_channel channel, ConsoleGeometry last_geom)
        : handles{handles}, channel{channel}, last_geometry{last_geom}
    {
    }
    void write_console();
    void read_console();
    void exit_console();
    void update_ssh_pty_size();
};

namespace
{
constexpr auto category = "windows console";
constexpr auto term_type = "xterm-256color";
constexpr auto chunk = 4096u;

mp::PlatformSSHClient::ConsoleGeometry get_console_size(mp::Handle handle)
{
    CONSOLE_SCREEN_BUFFER_INFO sb_info;
    GetConsoleScreenBufferInfo(handle, &sb_info);
    SHORT columns = sb_info.srWindow.Right - sb_info.srWindow.Left + 1;
    SHORT rows = sb_info.srWindow.Bottom - sb_info.srWindow.Top + 1;

    return {rows, columns}; // rows come before columns in ConsoleGeometry (unlike libssh)
}
} // namespace

mp::PlatformSSHClient::PlatformSSHClient(mp::PlainSSHSession&& ssh_session_obj, Terminal* term)
    : PlainSSHClient{std::move(ssh_session_obj)},
      term{term},
      p_data{std::make_unique<PlatformData>(term->native_handles(),
                                            channel.get(),
                                            get_console_size(term->native_handles().cout))},
      is_interactive{term->is_live()}
{
    setup_console();
}

mp::PlatformSSHClient::~PlatformSSHClient()
{
    // Previously called at the end of stdout_thread, which was not exception-safe
    restore_console();
    FreeConsole();
}

void mp::PlatformSSHClient::handle_ssh_events(ssh_session session)
{
    p_data->session_socket_fd = MP_LIBSSH.ssh_get_fd(session);
    // TODO: Protect against freeze? Look into external contributor progress
    auto stdout_thread = std::thread([this]() {
        while (MP_LIBSSH.ssh_channel_is_open(channel.get()) &&
               !MP_LIBSSH.ssh_channel_is_eof(channel.get()))
        {
            p_data->write_console();
        }
    });

    while (MP_LIBSSH.ssh_channel_is_open(channel.get()) &&
           !MP_LIBSSH.ssh_channel_is_eof(channel.get()))
    {
        p_data->read_console();
    }

    stdout_thread.join();
}

void mp::PlatformSSHClient::setup_console()
{
    if (is_interactive)
    {
        GetConsoleMode(p_data->handles.cin, &p_data->console_input_mode);
        SetConsoleMode(p_data->handles.cin,
                       p_data->console_input_mode & ~ENABLE_PROCESSED_INPUT & ~ENABLE_LINE_INPUT |
                           ENABLE_VIRTUAL_TERMINAL_INPUT | ENABLE_WINDOW_INPUT);
        GetConsoleMode(p_data->handles.cout, &p_data->console_output_mode);
        SetConsoleMode(p_data->handles.cout,
                       p_data->console_output_mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);

        MP_LIBSSH.ssh_channel_request_pty_size(channel.get(),
                                               term_type,
                                               p_data->last_geometry.columns,
                                               p_data->last_geometry.rows);
    }
}

void mp::PlatformSSHClient::PlatformData::read_console()
{
    std::array<INPUT_RECORD, chunk> input_records;
    DWORD num_records_read = 0;
    std::basic_string<CHAR> text_buffer;
    text_buffer.reserve(chunk);

    if (!ReadConsoleInput(handles.cin, input_records.data(), chunk, &num_records_read))
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
        MP_LIBSSH.ssh_channel_write(
            channel,
            text_buffer.data(),
            static_cast<uint32_t>(text_buffer.size() * sizeof(text_buffer.front())));
    }
}

void mp::PlatformSSHClient::PlatformData::write_console()
{
    DWORD write;
    std::array<char, chunk> buffer;
    int num_bytes{0};
    fd_set read_set;
    mp::Handle current_handle{handles.cout};

    FD_ZERO(&read_set);
    FD_SET(session_socket_fd, &read_set);

    if (select(0, &read_set, nullptr, nullptr, nullptr) < 1)
        return;

    {
        std::lock_guard<std::mutex> lock(ssh_mutex);
        num_bytes = MP_LIBSSH.ssh_channel_read_nonblocking(channel, buffer.data(), chunk, 0);

        // Try reading from stderr if nothing is returned from stdout
        if (num_bytes == 0)
        {
            num_bytes = MP_LIBSSH.ssh_channel_read_nonblocking(channel, buffer.data(), chunk, 1);
            current_handle = handles.cerr;
        }
    }

    if (num_bytes < 1)
    {
        // Force the channel to close if EOF is detected from the channel_read
        if (num_bytes == SSH_EOF)
            MP_LIBSSH.ssh_channel_close(channel);

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

void mp::PlatformSSHClient::restore_console()
{
    if (is_interactive)
    {
        SetConsoleMode(p_data->handles.cin, p_data->console_input_mode);
        SetConsoleMode(p_data->handles.cout, p_data->console_output_mode);
    }
}

void mp::PlatformSSHClient::PlatformData::update_ssh_pty_size()
{
    auto [rows, columns] = get_console_size(handles.cout);

    if (last_geometry.columns != columns || last_geometry.rows != rows)
    {
        last_geometry.columns = columns;
        last_geometry.rows = rows;
        std::lock_guard<std::mutex> lock(ssh_mutex);
        MP_LIBSSH.ssh_channel_change_pty_size(channel, columns, rows);
    }
}
