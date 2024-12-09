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

#ifndef MULTIPASS_TERMINAL_H
#define MULTIPASS_TERMINAL_H

#include <istream>
#include <libssh/libssh.h>
#include <memory>
#include <ostream>
#include <string>

namespace multipass
{
class Console;

class Terminal
{
public:
    virtual ~Terminal() = default;

    virtual std::istream& cin();
    virtual std::ostream& cout();
    virtual std::ostream& cerr();

    virtual bool cin_is_live() const = 0;
    virtual bool cout_is_live() const = 0;

    bool is_live() const;

    virtual std::string read_all_cin();
    virtual void set_cin_echo(const bool enable) = 0;

    using UPtr = std::unique_ptr<Terminal>;
    static UPtr make_terminal();

    using ConsolePtr = std::unique_ptr<Console>;
    virtual ConsolePtr make_console(ssh_channel channel) = 0;
};
} // namespace multipass

#endif // MULTIPASS_TERMINAL_H
