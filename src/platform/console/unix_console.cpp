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

#include "unix_console.h"

#include <multipass/auto_join_thread.h>
#include <multipass/platform_unix.h>
#include <multipass/terminal.h>

#include <sys/ioctl.h>
#include <unistd.h>

namespace mp = multipass;

namespace
{
const std::vector<int> blocked_sigs{SIGWINCH, SIGUSR1};
mp::Console::ConsoleGeometry last_geometry{0, 0};

void change_ssh_pty_size(ssh_channel channel, int cout_fd)
{
    struct winsize win = {0, 0, 0, 0};
    ioctl(cout_fd, TIOCGWINSZ, &win);

    if (last_geometry.rows != win.ws_row || last_geometry.columns != win.ws_col)
    {
        last_geometry.rows = win.ws_row;
        last_geometry.columns = win.ws_col;

        ssh_channel_change_pty_size(channel, win.ws_col, win.ws_row);
    }
}
}

class mp::WindowChangedSignalHandler
{
public:
    explicit WindowChangedSignalHandler(ssh_channel channel, int cout_fd)
        : signal_handling_thread{[this, channel, cout_fd] { monitor_signals(channel, cout_fd); }}
    {
    }

    ~WindowChangedSignalHandler()
    {
        pthread_kill(signal_handling_thread.thread.native_handle(), SIGUSR1);
    }

    void monitor_signals(ssh_channel channel, int cout_fd)
    {
        auto sigset{mp::platform::make_sigset(blocked_sigs)};

        while (true)
        {
            int sig = -1;
            sigwait(&sigset, &sig);

            if (sig == SIGWINCH)
            {
                change_ssh_pty_size(channel, cout_fd);
            }
            else
                break;
        }
    }

private:
    mp::AutoJoinThread signal_handling_thread;
};

mp::UnixConsole::UnixConsole(ssh_channel channel, Terminal* term)
    : term{term}, handler{std::make_unique<WindowChangedSignalHandler>(channel, term->cout_fd())}
{
    setup_console();

    if (term->cout_is_tty())
    {
        ssh_channel_request_pty(channel);
        change_ssh_pty_size(channel, term->cout_fd());
    }
}

mp::UnixConsole::~UnixConsole()
{
    restore_console();
}

void mp::UnixConsole::setup_environment()
{
    mp::platform::make_and_block_signals(blocked_sigs);
}

void mp::UnixConsole::setup_console()
{
    if (term->cin_is_tty())
    {
        struct termios terminal_local;

        tcgetattr(term->cin_fd(), &terminal_local);
        saved_terminal = terminal_local;
        cfmakeraw(&terminal_local);
        tcsetattr(term->cin_fd(), TCSANOW, &terminal_local);
    }
}

void mp::UnixConsole::restore_console()
{
    if (term->cin_is_tty())
    {
        tcsetattr(term->cin_fd(), TCSANOW, &saved_terminal);
    }
}
