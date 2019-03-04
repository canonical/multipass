/*
 * Copyright (C) 2019 Canonical, Ltd.
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
class UnixTerminal
{
public:
    virtual ~UnixTerminal();

    virtual std::istream& cin() override;
    virtual std::ostream& cout() override;
    virtual std::ostream& cerr() override;

    virtual int cin_fd() const override;
    virtual bool cin_is_tty() const override;

    virtual int cout_fd() const override;
    virtual bool cout_is_tty() const override;
};
} // namespace multipass

#endif // MULTIPASS_UNIX_TERMINAL_H
