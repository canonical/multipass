/*
 * Copyright (C) 2019 Canonical, Ltd.
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

#include "mock_settings.h"

namespace mpt = multipass::test;

using namespace testing;

void mpt::MockSettings::mockit()
{
    mpt::MockSingletonHelper<Settings, MockSettings, NiceMock>::mockit();
}

auto mpt::MockSettings::mock_instance() -> MockSettings&
{
    return dynamic_cast<MockSettings&>(instance());
}

void mpt::MockSettings::setup_mock_defaults()
{
    ON_CALL(*this, get(_)).WillByDefault([this](const auto& a) { return get_default(a); });
    ON_CALL(*this, set(_, _)).WillByDefault([this](const auto& a, const auto& /*ignored*/) { get_default(a); });
}
