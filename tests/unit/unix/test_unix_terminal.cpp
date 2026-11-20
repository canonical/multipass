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

#include <tests/unit/common.h>

#include "mock_libc_functions.h"

#include <src/platform/console/unix_terminal.h>

namespace mp = multipass;

using namespace testing;

namespace
{
struct TestUnixTerminal : public Test
{
    mp::UnixTerminal unix_terminal;
    const int fake_fd{42};
};
} // namespace

TEST_F(TestUnixTerminal, nativeHandlesReturnStdStreamFds)
{
    REPLACE(fileno, [](auto stream) {
        if (stream == stdin)
            return 10;
        if (stream == stdout)
            return 11;
        if (stream == stderr)
            return 12;
        return -1;
    });

    const auto handles = unix_terminal.native_handles();

    EXPECT_EQ(handles.cin, 10);
    EXPECT_EQ(handles.cout, 11);
    EXPECT_EQ(handles.cerr, 12);
}

TEST_F(TestUnixTerminal, isLiveReturnsTrueWhenTTY)
{
    REPLACE(fileno, [this](auto) { return fake_fd; });

    REPLACE(isatty, [this](auto fd) {
        EXPECT_EQ(fd, fake_fd);
        return 1;
    });

    EXPECT_TRUE(unix_terminal.cin_is_live());
    EXPECT_TRUE(unix_terminal.cout_is_live());
}

TEST_F(TestUnixTerminal, isLiveReturnsFalseWhenNotTTY)
{
    REPLACE(fileno, [this](auto) { return fake_fd; });

    REPLACE(isatty, [this](auto fd) {
        EXPECT_EQ(fd, fake_fd);
        return 0;
    });

    EXPECT_FALSE(unix_terminal.cin_is_live());
    EXPECT_FALSE(unix_terminal.cout_is_live());
}

TEST_F(TestUnixTerminal, setsEchoOnTerminal)
{
    REPLACE(fileno, [this](auto) { return fake_fd; });

    REPLACE(tcgetattr, [](auto...) { return 0; });

    REPLACE(tcsetattr, [](auto, auto, auto termios_p) {
        EXPECT_TRUE((termios_p->c_lflag & ECHO) == ECHO);
        return 0;
    });

    unix_terminal.set_cin_echo(true);
}

TEST_F(TestUnixTerminal, unsetsEchoOnTerminal)
{
    REPLACE(fileno, [this](auto) { return fake_fd; });

    REPLACE(tcgetattr, [](auto...) { return 0; });

    REPLACE(tcsetattr, [](auto, auto, auto termios_p) {
        EXPECT_FALSE((termios_p->c_lflag & ECHO) == ECHO);
        return 0;
    });

    unix_terminal.set_cin_echo(false);
}
