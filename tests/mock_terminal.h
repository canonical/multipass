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

#pragma once

#include "common.h"

#include <multipass/console.h>
#include <multipass/terminal.h>

namespace multipass
{
namespace test
{
struct MockTerminal : public Terminal
{
    MOCK_METHOD(std::istream&, cin, (), (override));
    MOCK_METHOD(std::ostream&, cout, (), (override));
    MOCK_METHOD(std::ostream&, cerr, (), (override));
    MOCK_METHOD(bool, cin_is_live, (), (const, override));
    MOCK_METHOD(bool, cout_is_live, (), (const, override));
    MOCK_METHOD(void, set_cin_echo, (const bool), (override));
    MOCK_METHOD(ConsolePtr, make_console, (ssh_channel), (override));
};
} // namespace test
} // namespace multipass
