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

// TODO: Set up cerr in Unix version
#include <multipass/platform_unix.h>
#include <multipass/ssh/libssh_wrapper.h>
#include <multipass/ssh/platform_ssh_client.h>
#include <multipass/terminal.h>

#include <sys/ioctl.h>

#include <atomic>
#include <csignal>
#include <cstdlib>
#include <termios.h>

namespace mp = multipass;

struct mp::PlatformSSHClient::PlatformData
{
    struct termios saved_terminal;
    struct sigaction winch_action;
    struct sigaction old_winch_action;
};

namespace
{

// Registered callbacks cannot use this* pointers, and so must access static data
mp::PlatformSSHClient::ConsoleGeometry local_pty_size{0, 0};
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

void handle_runtime_events(bool is_interactive)
{
    if (is_interactive && pty_size_changed.exchange(0, std::memory_order_relaxed) == 1 &&
        update_local_pty_size(global_cout_fd))
    {
        MP_LIBSSH.ssh_channel_change_pty_size(global_channel,
                                              local_pty_size.columns,
                                              local_pty_size.rows);
    }
}
} // namespace

mp::PlatformSSHClient::PlatformSSHClient(mp::PlainSSHSession&& ssh_session_obj, Terminal* term)
    : PlainSSHClient{std::move(ssh_session_obj)},
      term{term},
      p_data{std::make_unique<PlatformData>()},
      is_interactive{term->is_live()}
{
    const auto native_handles = term->native_handles();
    global_channel = channel.get();
    global_cout_fd = native_handles.cout;

    sigemptyset(&p_data->winch_action.sa_mask);
    p_data->winch_action.sa_flags = 0;
    p_data->winch_action.sa_handler = sigwinch_handler;

    if (is_interactive)
    {
        sigaction(SIGWINCH, &p_data->winch_action, &p_data->old_winch_action);

        const char* term_type = std::getenv("TERM");
        term_type = (term_type == nullptr) ? "xterm" : term_type;

        update_local_pty_size(native_handles.cout);
        MP_LIBSSH.ssh_channel_request_pty_size(channel.get(),
                                               term_type,
                                               local_pty_size.columns,
                                               local_pty_size.rows);

        // set stdin to Raw Mode after libssh inherits sane settings from stdin.
        setup_console();
    }
}

mp::PlatformSSHClient::~PlatformSSHClient()
{
    if (is_interactive)
    {
        sigaction(SIGWINCH, &p_data->old_winch_action, nullptr);
        restore_console();
    }
    global_channel = nullptr;
}

void mp::PlatformSSHClient::handle_ssh_events(ssh_session session)
{
    // TODO: Protect against freeze? Look into external contributor progress
    using ConnectorUPtr = std::unique_ptr<ssh_connector_struct, void (*)(ssh_connector)>;
    std::unique_ptr<ssh_event_struct, void (*)(ssh_event)> event{
        MP_LIBSSH.ssh_event_new(),
        [](ssh_event e) { MP_LIBSSH.ssh_event_free(e); }};

    const auto native_handles = term->native_handles();
    // stdin
    ConnectorUPtr connector_in{MP_LIBSSH.ssh_connector_new(session),
                               [](ssh_connector c) { MP_LIBSSH.ssh_connector_free(c); }};
    MP_LIBSSH.ssh_connector_set_out_channel(connector_in.get(),
                                            channel.get(),
                                            SSH_CONNECTOR_STDOUT);
    MP_LIBSSH.ssh_connector_set_in_fd(connector_in.get(), native_handles.cin);
    MP_LIBSSH.ssh_event_add_connector(event.get(), connector_in.get());

    // stdout
    ConnectorUPtr connector_out{MP_LIBSSH.ssh_connector_new(session),
                                [](ssh_connector c) { MP_LIBSSH.ssh_connector_free(c); }};
    MP_LIBSSH.ssh_connector_set_out_fd(connector_out.get(), native_handles.cout);
    MP_LIBSSH.ssh_connector_set_in_channel(connector_out.get(),
                                           channel.get(),
                                           SSH_CONNECTOR_STDOUT);
    MP_LIBSSH.ssh_event_add_connector(event.get(), connector_out.get());

    // stderr
    ConnectorUPtr connector_err{MP_LIBSSH.ssh_connector_new(session),
                                [](ssh_connector c) { MP_LIBSSH.ssh_connector_free(c); }};
    MP_LIBSSH.ssh_connector_set_out_fd(connector_err.get(), native_handles.cerr);
    MP_LIBSSH.ssh_connector_set_in_channel(connector_err.get(),
                                           channel.get(),
                                           SSH_CONNECTOR_STDERR);
    MP_LIBSSH.ssh_event_add_connector(event.get(), connector_err.get());

    while (MP_LIBSSH.ssh_channel_is_open(channel.get()) &&
           !MP_LIBSSH.ssh_channel_is_eof(channel.get()))
    {
        MP_LIBSSH.ssh_event_dopoll(event.get(), 60000);
        handle_runtime_events(is_interactive);
    }

    MP_LIBSSH.ssh_event_remove_connector(event.get(), connector_in.get());
    MP_LIBSSH.ssh_event_remove_connector(event.get(), connector_out.get());
    MP_LIBSSH.ssh_event_remove_connector(event.get(), connector_err.get());
}

void mp::PlatformSSHClient::setup_console()
{
    struct termios terminal_local;

    const auto cin_fd = term->native_handles().cin;
    tcgetattr(cin_fd, &terminal_local);
    p_data->saved_terminal = terminal_local;
    cfmakeraw(&terminal_local);
    tcsetattr(cin_fd, TCSANOW, &terminal_local);
}

void mp::PlatformSSHClient::restore_console()
{
    const auto cin_fd = term->native_handles().cin;
    tcsetattr(cin_fd, TCSANOW, &p_data->saved_terminal);
}
