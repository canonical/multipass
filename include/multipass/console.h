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

#ifndef MULTIPASS_CONSOLE_H
#define MULTIPASS_CONSOLE_H

#include "disabled_copy_move.h"

#include <libssh/libssh.h>

#include <array>
#include <functional>
#include <memory>

namespace multipass
{
class Console : private DisabledCopyMove
{
public:
    struct ConsoleGeometry
    {
        int rows;
        int columns;
    };

    using UPtr = std::unique_ptr<Console>;

    virtual ~Console() = default;

    virtual void read_console() = 0;
    virtual void write_console() = 0;
    virtual void exit_console() = 0;

    static void setup_environment();

protected:
    explicit Console() = default;
};
} // namespace multipass
#endif // MULTIPASS_CONSOLE_H
