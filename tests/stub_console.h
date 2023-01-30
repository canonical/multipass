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

#ifndef MULTIPASS_STUB_CONSOLE_H
#define MULTIPASS_STUB_CONSOLE_H

#include <multipass/console.h>

namespace multipass
{
namespace test
{
struct StubConsole final : public multipass::Console
{
    void read_console() override{};

    void write_console() override{};

    void exit_console() override{};
};
}
}
#endif // MULTIPASS_STUB_CONSOLE_H
