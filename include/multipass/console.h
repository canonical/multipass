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

#ifndef MULTIPASS_CONSOLE_H
#define MULTIPASS_CONSOLE_H

#include <libssh/libssh.h>

#include <array>
#include <functional>
#include <memory>

namespace multipass
{
class Console
{
public:
    using UPtr = std::unique_ptr<Console>;

    virtual ~Console() = default;

    virtual int read_console(std::array<char, 512>& buffer) = 0;
    virtual void write_console(const char* buffer, int bytes) = 0;
    virtual void signal_console() = 0;

    static UPtr make_console(ssh_channel channel);
    static void setup_environment();

protected:
    explicit Console() = default;
    Console(const Console&) = delete;
    Console& operator=(const Console&) = delete;
};
}
#endif // MULTIPASS_CONSOLE_H
