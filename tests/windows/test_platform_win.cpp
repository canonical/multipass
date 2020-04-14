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

#include <QTemporaryFile>

#include <json/json.h>

#include <scope_guard.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <fstream>
#include <sstream>
#include <utility>

namespace mp = multipass;
namespace mpl = mp::logging;
namespace mpt = mp::test;
using namespace testing;

namespace
{

void mock_winterm_setting(const QString& ret)
{
    EXPECT_CALL(mpt::MockSettings::mock_instance(), get(Eq(mp::winterm_key))).WillOnce(Return(ret));
}

void mock_stdpaths_locate(const QString& ret)
{
    EXPECT_CALL(mpt::MockStandardPaths::mock_instance(),
                locate(_, Property(&QString::toStdString, EndsWith("profiles.json")), _))
        .WillOnce(Return(ret));
}

auto guarded_mock_logger()
{
    auto guard = sg::make_scope_guard([] { mpl::set_logger(nullptr); });
    auto mock_logger = std::make_shared<StrictMock<mpt::MockLogger>>();
    mpl::set_logger(mock_logger);

    return std::make_pair(mock_logger, std::move(guard)); // needs to be moved into the pair first (NRVO does not apply)
}

auto expect_log(mpl::Level lvl, const std::string& substr)
{
    auto [mock_logger, guard] = guarded_mock_logger();

    EXPECT_CALL(*mock_logger, log(lvl, _, mpt::MockLogger::make_cstring_matcher(HasSubstr(substr))));

    return std::move(guard); // needs to be moved because it's only part of an implicit local var (NRVO does not apply)
}

// ptr works around uncopyable QTemporaryFile
std::unique_ptr<QTemporaryFile> fake_json(const char* contents)
{
    auto json_file = std::make_unique<QTemporaryFile>();
    EXPECT_TRUE(json_file->open());   // can't use gtest's asserts in non-void function
    EXPECT_TRUE(json_file->exists()); // idem

    json_file->write(contents);
    json_file->close();

    mock_stdpaths_locate(json_file->fileName());

    return json_file;
}

// ptr works around uncopyable QTemporaryFile
std::unique_ptr<QTemporaryFile> fake_json(const Json::Value& json)
{
    std::ostringstream oss;
    oss << json;
    const auto data = oss.str();

    return fake_json(data.c_str());
}

Json::Value read_json(const QString& filename)
{
    std::ifstream ifs{filename.toStdString(), std::ifstream::binary};
    EXPECT_TRUE(ifs); // can't use gtest's asserts in non-void function

    Json::Value json;
    EXPECT_NO_THROW(ifs >> json); // idem

    return json;
}

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

TEST(PlatformWin, winterm_sync_warns_if_setting_primary_but_no_file)
{
    mock_winterm_setting("primary");
    mock_stdpaths_locate("");
    auto mock_logger_guard = expect_log(mpl::Level::warning, "Could not find");

    mp::platform::sync_winterm_profiles();
}

TEST(PlatformWin, winterm_sync_ignores_if_setting_off_and_no_file)
{
    mock_winterm_setting("none");
    mock_stdpaths_locate("");
    auto [mock_logger, guard] = guarded_mock_logger();

    EXPECT_CALL(*mock_logger, log(_, _, _)).Times(0);
    mp::platform::sync_winterm_profiles();
}

struct TestWinTermSyncLogging : public TestWithParam<std::pair<QString, mpl::Level>>
{
};

TEST_P(TestWinTermSyncLogging, logging_on_unreadable_settings)
{
    const auto& [setting, lvl] = GetParam();

    mock_winterm_setting(setting);
    mock_stdpaths_locate("C:\\unreadable\\profiles.json");
    auto mock_logger_guard = expect_log(lvl, "Could not read");

    mp::platform::sync_winterm_profiles();
}

TEST_P(TestWinTermSyncLogging, logging_on_unparseable_settings)
{
    const auto& [setting, lvl] = GetParam();
    mock_winterm_setting(setting);

    auto json_file = fake_json("~!@#$% rubbish ^&*()_+");
    auto mock_logger_guard = expect_log(lvl, "Could not parse");

    mp::platform::sync_winterm_profiles();
}

INSTANTIATE_TEST_SUITE_P(PlatformWin, TestWinTermSyncLogging,
                         Values(std::make_pair(QStringLiteral("none"), mpl::Level::info),
                                std::make_pair(QStringLiteral("primary"), mpl::Level::error)));

TEST(PlatformWin, winterm_sync_keeps_visible_profile_if_setting_primary)
{
    mock_winterm_setting("primary");
    const auto guarded_logger = guarded_mock_logger(); // strict mock expects no calls

    Json::Value json;
    json["profiles"]["list"][0]["guid"] = mp::winterm_profile_guid;
    json["profiles"]["list"][0]["hidden"] = false;
    const auto json_file = fake_json(json);

    mp::platform::sync_winterm_profiles();
    EXPECT_EQ(json, read_json(json_file->fileName()));
}
} // namespace
