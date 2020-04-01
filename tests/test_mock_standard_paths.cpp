/*
 * Copyright (C) 2020 Canonical, Ltd.
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

#include "mock_standard_paths.h"

#include <multipass/standard_paths.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace mp = multipass;
namespace mpt = mp::test;
using namespace testing;

namespace
{

TEST(StandardPaths, can_be_mocked)
{
    [[maybe_unused]] // TODO@ricab remove
    const auto& mock = mpt::MockStandardPaths::mock_instance();

    // TODO@ricab actually test
}

} // namespace
