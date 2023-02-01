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

#include "mock_standard_paths.h"

namespace mpt = multipass::test;

using namespace testing;

void mpt::MockStandardPaths::mockit()
{
    mpt::MockSingletonHelper<MockStandardPaths, NiceMock>::mockit();
}

auto mpt::MockStandardPaths::mock_instance() -> MockStandardPaths&
{
    return dynamic_cast<MockStandardPaths&>(instance());
}

void mpt::MockStandardPaths::setup_mock_defaults()
{
    // see https://github.com/google/googletest/issues/2794
    ON_CALL(*this, locate(_, _, _)).WillByDefault([this](const auto& a, const auto& b, const auto& c) {
        return StandardPaths::locate(a, b, c); // call the parent (this implicit)
    });
    ON_CALL(*this, standardLocations(_)).WillByDefault([this](const auto& loc) {
        return StandardPaths::standardLocations(loc); // call the parent (this implicit)
    });
    ON_CALL(*this, writableLocation(_)).WillByDefault([this](const auto& loc) {
        return StandardPaths::writableLocation(loc); // call the parent (this implicit)
    });
}
