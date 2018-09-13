/*
 * Copyright (C) 2018 Canonical, Ltd.
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

#include "test_with_mocked_bin_path.h"
#include "path.h"

#include <cstdlib>

namespace mp = multipass;
namespace mpt = multipass::test;
using namespace testing;

namespace
{
void set_path(const std::string& value)
{
    auto ret = setenv("PATH", value.c_str(), 1);
    if (ret != 0)
    {
        std::string message{"unable to modify PATH env variable err:"};
        message.append(std::to_string(ret));
        throw std::runtime_error(message);
    }
}
} // namespace

void mpt::TestWithMockedBinPath::SetUp()
{
    old_path = getenv("PATH");
    std::string new_path{mpt::mock_bin_path()};
    new_path.append(":");
    new_path.append(old_path);
    set_path(new_path);
}

void mpt::TestWithMockedBinPath::TearDown()
{
    set_path(old_path);
}
