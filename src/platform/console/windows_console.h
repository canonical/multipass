/*
 * Copyright (C) 2017 Canonical, Ltd.
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

#include <windows.h>

namespace multipass
{
class WindowsConsole final : public Console
{
public:
    WindowsConsole();
    ~WindowsConsole();

    void setup_console() override;
    int read_console(std::array<char, 512>& buffer) override;
    void write_console(const char* buffer, int bytes) override;
    void signal_console() override;
    bool is_window_size_changed() override;
    bool is_interactive() override;
    WindowGeometry get_window_geometry() override;

private:
    void restore_console() override;

    bool interactive;
    HANDLE input_handle;
    HANDLE output_handle;
    DWORD console_input_mode;
    DWORD console_output_mode;
    WindowGeometry saved_geometry;
    HANDLE events[2];
};
}
#endif // MULTIPASS_WINDOWS_CONSOLE_H
