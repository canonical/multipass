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

#ifndef MULTIPASS_MOCK_STDCIN_H
#define MULTIPASS_MOCK_STDCIN_H

#include <iostream>
#include <sstream>

class MockStdCin
{
public:
    MockStdCin(const std::string& s)
    {
        fake_cin << s;

        // Backup and replace std::cin's streambuffer
        cin_backup = std::cin.rdbuf();
        std::streambuf* psbuf = fake_cin.rdbuf();
        std::cin.rdbuf(psbuf); // assign streambuf to cin
    }

    ~MockStdCin()
    {
        // Restore cin's original streanbuffer
        std::cin.rdbuf(cin_backup);
    }

private:
    std::streambuf* cin_backup;
    std::stringstream fake_cin;
};

#endif // MULTIPASS_MOCK_STDCIN_H
