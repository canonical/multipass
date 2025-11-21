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

#ifdef MULTIPASS_PLATFORM_WINDOWS
#include "windows_console.h"
#include "windows_terminal.h"
#else
#include "unix_console.h"
#endif

namespace mp = multipass;

void mp::Console::setup_environment()
{
#ifndef MULTIPASS_PLATFORM_WINDOWS
    UnixConsole::setup_environment();
#endif
}
