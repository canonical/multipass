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

#include <multipass/console.h>
#include <multipass/terminal.h>

#include "unix_console.h"
#include "unix_terminal.h"

namespace mp = multipass;

mp::Console::UPtr mp::Console::make_console(ssh_channel channel, Terminal* term)
{
    return std::make_unique<UnixConsole>(channel, static_cast<UnixTerminal*>(term));
}

void mp::Console::setup_environment()
{
    UnixConsole::setup_environment();
}
