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

#include "unix_terminal.h"

#include <termios.h>
#include <unistd.h>

namespace mp = multipass;

namespace
{
int cin_fd()
{
    return fileno(stdin);
}

int cout_fd()
{
    return fileno(stdout);
}

int cerr_fd()
{
    return fileno(stderr);
}
} // namespace

bool mp::UnixTerminal::cin_is_live() const
{
    return (isatty(cin_fd()) == 1);
}

bool mp::UnixTerminal::cout_is_live() const
{
    return (isatty(cout_fd()) == 1);
}

mp::NativeHandles mp::UnixTerminal::native_handles() const
{
    return {.cin = cin_fd(), .cout = cout_fd(), .cerr = cerr_fd()};
}

void mp::UnixTerminal::set_cin_echo(const bool enable)
{
    struct termios tty;
    tcgetattr(cin_fd(), &tty);

    if (!enable)
        tty.c_lflag &= ~ECHO;
    else
        tty.c_lflag |= ECHO;

    tcsetattr(cin_fd(), TCSANOW, &tty);
}
