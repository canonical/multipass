/*
 * Copyright (C) 2019-2020 Canonical, Ltd.
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

#include <multipass/constants.h>
#include <multipass/settings.h>

#include <QString>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace mp = multipass;
namespace mpt = mp::test;
using namespace testing;

namespace
{

TEST(Settings, provides_get_default_as_get_by_default)
{
    const auto& key = mp::driver_key;
    ASSERT_EQ(mp::Settings::instance().get(key), mpt::MockSettings::mock_instance().get_default(key));
}

TEST(Settings, can_have_get_mocked)
{
    const auto test = QStringLiteral("abc"), proof = QStringLiteral("xyz");
    auto& mock = mpt::MockSettings::mock_instance();

    EXPECT_CALL(mock, get(test)).WillOnce(Return(proof));
    ASSERT_EQ(mp::Settings::instance().get(test), proof);
}

} // namespace
