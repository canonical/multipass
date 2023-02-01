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

#ifndef MULTIPASS_UNIX_CONSOLE_H
#define MULTIPASS_UNIX_CONSOLE_H

#include <multipass/console.h>

#include <libssh/libssh.h>

#include <vector>

#include <csignal>
#include <termios.h>
#include "unix_terminal.h"

namespace multipass
{
class UnixConsole final : public Console
{
public:
    explicit UnixConsole(ssh_channel channel, UnixTerminal *term);
    ~UnixConsole();

    void read_console() override{};
    void write_console() override{};
    void exit_console() override{};

    static void setup_environment();

private:
    void setup_console();
    void restore_console();

    UnixTerminal* term;
    struct termios saved_terminal;
    struct sigaction winch_action;
};
} // namespace multipass
#endif // MULTIPASS_UNIX_CONSOLE_H
