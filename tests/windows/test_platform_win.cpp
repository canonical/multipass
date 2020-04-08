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

#include "tests/extra_assertions.h"
#include "tests/mock_logger.h"
#include "tests/mock_settings.h"
#include "tests/mock_standard_paths.h"

#include <multipass/constants.h>
#include <multipass/exceptions/settings_exceptions.h>
#include <multipass/logging/log.h>
#include <multipass/platform.h>

#include <scope_guard.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace mp = multipass;
namespace mpl = mp::logging;
namespace mpt = mp::test;
using namespace testing;

namespace
{

TEST(PlatformWin, winterm_in_extra_settings)
{
    EXPECT_THAT(mp::platform::extra_settings_defaults(), Contains(Pair(Eq(mp::winterm_key), _)));
}

TEST(PlatformWin, valid_winterm_setting_values)
{
    for (const auto x : {"none", "primary"})
        EXPECT_EQ(mp::platform::interpret_winterm_integration(x), x);
}

TEST(PlatformWin, winterm_setting_values_case_insensitive)
{
    for (const auto x : {"NoNe", "NONE", "nonE", "NonE"})
        EXPECT_EQ(mp::platform::interpret_winterm_integration(x), "none");

    for (const auto x : {"pRIMARY", "Primary", "pRimarY"})
        EXPECT_EQ(mp::platform::interpret_winterm_integration(x), "primary");
}

TEST(PlatformWin, unsupported_winterm_setting_values_cause_exception)
{
    for (const auto x : {"Unsupported", "values", "1", "000", "false", "True", "", "  "})
        MP_EXPECT_THROW_THAT(
            mp::platform::interpret_winterm_integration(x), mp::InvalidSettingsException,
            Property(&mp::InvalidSettingsException::what,
                     AllOf(HasSubstr(mp::winterm_key), HasSubstr(x), HasSubstr("none"), HasSubstr("primary"))));
}

TEST(PlatformWin, winterm_sync_warns_if_setting_is_primary_but_no_file)
{
    auto& mock_settings = mpt::MockSettings::mock_instance();
    EXPECT_CALL(mock_settings, get(Eq(mp::winterm_key))).WillOnce(Return("primary"));

    auto& mock_stdpaths = mpt::MockStandardPaths::mock_instance();
    EXPECT_CALL(mock_stdpaths, locate(_, Property(&QString::toStdString, EndsWith("profiles.json")), _))
        .WillOnce(Return(""));

    auto guard = sg::make_scope_guard([] { mpl::set_logger(nullptr); });
    auto mock_logger = std::make_shared<StrictMock<mpt::MockLogger>>();
    mpl::set_logger(mock_logger);

    EXPECT_CALL(*mock_logger,
                log(mpl::Level::warning, _, mpt::MockLogger::make_cstring_matcher(HasSubstr("Could not find"))));
    mp::platform::sync_winterm_profiles();
}

TEST(PlatformWin, winterm_sync_ignores_if_setting_off_and_no_file)
{
    auto& mock_settings = mpt::MockSettings::mock_instance();
    EXPECT_CALL(mock_settings, get(Eq(mp::winterm_key))).WillOnce(Return("none"));

    auto& mock_stdpaths = mpt::MockStandardPaths::mock_instance();
    EXPECT_CALL(mock_stdpaths, locate(_, Property(&QString::toStdString, EndsWith("profiles.json")), _))
        .WillOnce(Return(""));

    auto guard = sg::make_scope_guard([] { mpl::set_logger(nullptr); });
    auto mock_logger = std::make_shared<mpt::MockLogger>();
    mpl::set_logger(mock_logger);

    EXPECT_CALL(*mock_logger, log(_, _, _)).Times(0);
    mp::platform::sync_winterm_profiles();
}

} // namespace
