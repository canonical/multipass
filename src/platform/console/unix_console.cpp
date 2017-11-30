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

#include "unix_console.h"

#include <atomic>

#include <signal.h>
#include <sys/ioctl.h>
#include <unistd.h>

namespace mp = multipass;

namespace
{
std::atomic<bool> window_resized{false};

bool was_set(std::atomic<bool>& value)
{
    bool old = value;
    while (old && !value.compare_exchange_weak(old, false))
    {
    }
    return old;
}

static void sig_window_changed(int i)
{
    (void)i;
    window_resized = true;
}

void set_signal()
{
    signal(SIGWINCH, sig_window_changed);
}
}

mp::UnixConsole::UnixConsole() : interactive{isatty(fileno(stdin)) == 1}
{
    set_signal();
}

mp::UnixConsole::~UnixConsole()
{
    restore_console();
}

void mp::UnixConsole::setup_console()
{
    if (interactive)
    {
        struct termios terminal_local;

        tcgetattr(fileno(stdin), &terminal_local);
        saved_terminal = terminal_local;
        cfmakeraw(&terminal_local);
        tcsetattr(fileno(stdin), TCSANOW, &terminal_local);
    }
}

bool mp::UnixConsole::is_window_size_changed()
{
    return was_set(window_resized);
}

mp::Console::WindowGeometry mp::UnixConsole::get_window_geometry()
{
    struct winsize win = {0, 0, 0, 0};
    ioctl(fileno(stdout), TIOCGWINSZ, &win);

    return mp::Console::WindowGeometry{win.ws_row, win.ws_col};
}

bool mp::UnixConsole::is_interactive()
{
    return interactive;
}

void mp::UnixConsole::restore_console()
{
    if (interactive)
    {
        tcsetattr(fileno(stdin), TCSANOW, &saved_terminal);
    }
}
