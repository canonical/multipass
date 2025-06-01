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

#pragma once

#include <multipass/console.h>

#include <libssh/libssh.h>

#include <mutex>
#include <thread>

#include <windows.h>

namespace multipass
{
class WindowsTerminal;

class WindowsConsole final : public Console
{
public:
    WindowsConsole(ssh_channel channel, WindowsTerminal* term);

    void read_console() override;
    void write_console() override;
    void exit_console() override;

private:
    void setup_console();
    void restore_console();
    void update_ssh_pty_size();

    bool interactive{false};
    HANDLE input_handle;
    HANDLE output_handle;
    HANDLE error_handle;
    DWORD console_input_mode;
    DWORD console_output_mode;
    ssh_channel channel;
    socket_t session_socket_fd;
    ConsoleGeometry last_geometry;
    mutable std::mutex ssh_mutex;
};
} // namespace multipass
