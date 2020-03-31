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

#include <multipass/exceptions/settings_exceptions.h>
#include <multipass/platform.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace mp = multipass;
using namespace testing;

namespace
{

TEST(PlatformOSX, test_no_extra_settings)
{
    EXPECT_THAT(mp::platform::extra_settings_defaults(), IsEmpty());
}

TEST(PlatformOSX, test_winterm_setting_not_supported)
{
    for (const auto x : {"no", "matter", "what"})
        EXPECT_THROW(mp::platform::interpret_winterm_integration(x), mp::InvalidSettingsException);
}

} // namespace
