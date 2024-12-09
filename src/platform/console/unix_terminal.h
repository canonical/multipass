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

#ifndef MULTIPASS_UNIX_TERMINAL_H
#define MULTIPASS_UNIX_TERMINAL_H

#include <multipass/terminal.h>

namespace multipass
{
class UnixTerminal : public Terminal
{
public:
    int cin_fd() const;
    bool cin_is_live() const override;

    int cout_fd() const;
    bool cout_is_live() const override;

    void set_cin_echo(const bool enable) override;

    ConsolePtr make_console(ssh_channel channel) override;
};
} // namespace multipass

#endif // MULTIPASS_UNIX_TERMINAL_H
