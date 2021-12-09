/*
 * Copyright (C) 2021 Canonical, Ltd.
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

#ifndef MULTIPASS_MOCK_TERMINAL_H
#define MULTIPASS_MOCK_TERMINAL_H

#include "common.h"

#include <multipass/terminal.h>

namespace multipass
{
namespace test
{
struct MockTerminal : public Terminal
{
    MOCK_METHOD0(cin, std::istream&());
    MOCK_METHOD0(cout, std::ostream&());
    MOCK_METHOD0(cerr, std::ostream&());
    MOCK_CONST_METHOD0(cin_is_live, bool());
    MOCK_CONST_METHOD0(cout_is_live, bool());
    MOCK_METHOD1(set_cin_echo, void(const bool));
};
} // namespace test
} // namespace multipass
#endif // MULTIPASS_MOCK_TERMINAL_H
