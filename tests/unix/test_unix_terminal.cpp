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

#include <tests/common.h>

#include "mock_libc_functions.h"

#include <src/platform/console/unix_console.h>
#include <src/platform/console/unix_terminal.h>

#include <tuple>

namespace mp = multipass;
namespace mpt = multipass::test;

using namespace testing;

namespace
{
struct TestUnixTerminal : public Test
{
    mp::UnixTerminal unix_terminal;
    const int fake_fd{42};
};
} // namespace

TEST_F(TestUnixTerminal, cinFdReturnsExpectedFd)
{
    REPLACE(fileno, [this](auto stream) {
        EXPECT_EQ(stream, stdin);
        return fake_fd;
    });

    EXPECT_EQ(unix_terminal.cin_fd(), fake_fd);
}

TEST_F(TestUnixTerminal, coutFdReturnsExpectedFd)
{
    REPLACE(fileno, [this](auto stream) {
        EXPECT_EQ(stream, stdout);
        return fake_fd;
    });

    EXPECT_EQ(unix_terminal.cout_fd(), fake_fd);
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

TEST_F(TestUnixTerminal, make_console_makes_unix_console)
{
    // force is_live() to return false so UnixConsole ctor doesn't break
    REPLACE(fileno, [this](auto) { return fake_fd; });
    REPLACE(isatty, [this](auto fd) {
        EXPECT_EQ(fd, fake_fd);
        return 0;
    });

    auto console = unix_terminal.make_console(nullptr);
    EXPECT_NE(dynamic_cast<multipass::UnixConsole*>(console.get()), nullptr);
}
