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

#include <multipass/constants.h>
#include <multipass/platform.h>

#include <src/platform/platform_shared.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <QKeySequence>

namespace mp = multipass;
using namespace testing;

namespace
{

TEST(PlatformShared, hotkey_in_extra_settings) // TODO@ricardo replicate in macos, run in windows
{
    EXPECT_THAT(mp::platform::extra_settings_defaults(), Contains(Pair(Eq(mp::hotkey_key), _)));
}

TEST(PlatformShared, default_hotkey_presentation_is_normalized) // TODO@ricardo replicate in macos, run in windows
{
    for (const auto& [k, v] : mp::platform::extra_settings_defaults())
    {
        if (k == mp::hotkey_key)
        {
            EXPECT_EQ(v, QKeySequence{v}.toString());
        }
    }
}
} // namespace
