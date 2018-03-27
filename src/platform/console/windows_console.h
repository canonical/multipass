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

#ifndef MULTIPASS_WINDOWS_CONSOLE_H
#define MULTIPASS_WINDOWS_CONSOLE_H

#include <multipass/console.h>

#include <libssh/libssh.h>

#include <windows.h>

namespace multipass
{
class WindowsConsole final : public Console
{
public:
    WindowsConsole(ssh_channel channel);
    ~WindowsConsole();

    int read_console(std::array<char, 512>& buffer) override;
    void write_console(const char* buffer, int bytes) override;
    void signal_console() override;

private:
    struct WindowGeometry
    {
        int rows;
        int columns;
    };

    void setup_console();
    void restore_console();
    void change_ssh_pty_size();

    bool interactive{false};
    HANDLE input_handle;
    HANDLE output_handle;
    DWORD console_input_mode;
    DWORD console_output_mode;
    WindowGeometry saved_geometry;
    HANDLE events[2];
    ssh_channel channel;
};
}
#endif // MULTIPASS_WINDOWS_CONSOLE_H
