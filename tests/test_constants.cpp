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

#include "common.h"

#include <multipass/constants.h>
#include <multipass/memory_size.h>

namespace mp = multipass;
using namespace testing;

TEST(Constants, constants_constraints) {
    EXPECT_NO_THROW(std::stoi(mp::min_cpu_cores));
    EXPECT_NO_THROW(std::stoi(mp::default_cpu_cores));
    EXPECT_NO_THROW(mp::MemorySize{mp::min_memory_size});
    EXPECT_NO_THROW(mp::MemorySize{mp::default_memory_size});
    EXPECT_NO_THROW(mp::MemorySize{mp::min_disk_size});
    EXPECT_NO_THROW(mp::MemorySize{mp::default_disk_size});
 }

TEST(Constants, defaults_greater_or_equal_to_minimums)
{
    EXPECT_GE(mp::MemorySize{mp::default_memory_size}, mp::MemorySize{mp::min_memory_size});
    EXPECT_GE(mp::MemorySize{mp::default_disk_size}, mp::MemorySize{mp::min_disk_size});
    EXPECT_GE(std::stoi(mp::default_cpu_cores), std::stoi(mp::min_cpu_cores));
}

