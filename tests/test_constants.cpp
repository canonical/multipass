/*
 * Copyright (C) 2017-2019 Canonical, Ltd.
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

#include <multipass/constants.h>

#include <gtest/gtest.h>

namespace mp = multipass;
using namespace testing;

TEST(Constants, constants_constraints) {
    EXPECT_NO_THROW(std::stoi(mp::min_cpu_cores));
    EXPECT_NO_THROW(std::stoi(mp::default_cpu_cores));
}

