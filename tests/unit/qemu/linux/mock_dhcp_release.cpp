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

#include <fstream>
#include <string>

namespace
{
bool should_fail(const std::string& name)
{
    std::string fail{".fail"};
    return std::equal(fail.rbegin(), fail.rend(), name.rbegin());
}
} // namespace

int main(int argc, char* argv[])
{
    if (argc != 4)
        return EXIT_FAILURE;

    const std::string hw_addr{argv[3]};
    if (hw_addr == "00:00:00:00:00:00")
    {
        abort();
    }

    // The first argument describes a file path which serves as a signal
    // to the test using this mocked binary
    std::string path{argv[1]};
    std::ofstream out{path};
    out << "called\n";

    return should_fail(path) ? 1 : 0;
}
