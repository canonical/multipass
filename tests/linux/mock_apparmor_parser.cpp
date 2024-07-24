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

#include <cstring>
#include <fstream>
#include <iostream>
#include <string>

using namespace std;

int main(int argc, char* argv[])
{
    // looks for version just to ensure existence
    if (argc == 2 && strcmp(argv[1], "-V") == 0)
    {
        cout << "AppArmor parser version 1.11" << endl;
        return 0;
    }

    // stick all input in a file named /tmp/multipass-apparmor-profile.txt
    fstream out("/tmp/multipass-apparmor-profile.txt", fstream::out);

    out << "args: ";
    for (int i = 1; i < argc; i++)
    {
        out << argv[i] << ", ";
    }
    out << endl;
    string s;
    std::getline(cin, s, '\0');
    out << s;
    return 0;
}
