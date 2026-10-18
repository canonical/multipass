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

#include "unix_console.h"

#include <multipass/platform_unix.h>
#include <multipass/ssh/libssh_wrapper.h>

#include <sys/ioctl.h>

#include <atomic>
#include <cstdlib>

namespace mp = multipass;

namespace
{
mp::Console::ConsoleGeometry local_pty_size{0, 0};
ssh_channel global_channel;
int global_cout_fd;
std::atomic<std::sig_atomic_t> pty_size_changed{0};
static_assert(pty_size_changed.is_always_lock_free,
              "Fatal: pty_size_changed is not lock-free! Unsafe for signal handlers.");

bool update_local_pty_size(int cout_fd)
{
    struct winsize win = {0, 0, 0, 0};
    ioctl(cout_fd, TIOCGWINSZ, &win);

    bool local_pty_size_changed = local_pty_size.rows != win.ws_row ||
                                  local_pty_size.columns != win.ws_col;

    if (local_pty_size_changed)
    {
        local_pty_size.rows = win.ws_row;
        local_pty_size.columns = win.ws_col;
    }

    return local_pty_size_changed;
}

static void sigwinch_handler(int sig)
{
    if (sig == SIGWINCH)
    {
        pty_size_changed.store(1, std::memory_order_relaxed);
    }
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

    if (term->is_live())
    {
        const char* term_type = std::getenv("TERM");
        term_type = (term_type == nullptr) ? "xterm" : term_type;

        update_local_pty_size(term->cout_fd());
        MP_LIBSSH.ssh_channel_request_pty_size(channel,
                                               term_type,
                                               local_pty_size.columns,
                                               local_pty_size.rows);

        // set stdin to Raw Mode after libssh inherits sane settings from stdin.
        setup_console();
    }
}

mp::UnixConsole::~UnixConsole()
{
    if (term->is_live())
    {
        restore_console();
    }
}

void mp::UnixConsole::handle_runtime_events()
{
    if (pty_size_changed.exchange(0, std::memory_order_relaxed) == 1 &&
        update_local_pty_size(global_cout_fd))
    {
        MP_LIBSSH.ssh_channel_change_pty_size(global_channel,
                                              local_pty_size.columns,
                                              local_pty_size.rows);
    }
}

void mp::UnixConsole::setup_console()
{
    struct termios terminal_local;

    tcgetattr(term->cin_fd(), &terminal_local);
    saved_terminal = terminal_local;
    cfmakeraw(&terminal_local);
    tcsetattr(term->cin_fd(), TCSANOW, &terminal_local);
}

void mp::UnixConsole::restore_console()
{
    tcsetattr(term->cin_fd(), TCSANOW, &saved_terminal);
}
