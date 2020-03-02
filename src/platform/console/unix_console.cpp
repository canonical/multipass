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
mp::Console::ConsoleGeometry last_geometry{0, 0};
ssh_channel global_channel;
int global_cout_fd;

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

static void sigwinch_handler(int sig)
{
    if (sig == SIGWINCH)
        change_ssh_pty_size(global_channel, global_cout_fd);
}
} // namespace

mp::UnixConsole::UnixConsole(ssh_channel channel, UnixTerminal* term) : term{term}
{
    global_channel = channel;
    global_cout_fd = term->cout_fd();

    sigemptyset(&winch_action.sa_mask);
    winch_action.sa_flags = 0;
    winch_action.sa_handler = sigwinch_handler;
    sigaction(SIGWINCH, &winch_action, nullptr);

    setup_console();

    if (term->is_live())
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
}

void mp::UnixConsole::setup_console()
{
    if (term->is_live())
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
    if (term->is_live())
    {
        tcsetattr(term->cin_fd(), TCSANOW, &saved_terminal);
    }
}
