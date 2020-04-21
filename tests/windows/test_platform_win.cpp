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

struct TestWinTermSyncLesserLogging : public TestWithParam<std::pair<QString, mpl::Level>>
{
};

TEST_P(TestWinTermSyncLesserLogging, logging_on_no_file)
{
    const auto& [setting, lvl] = GetParam();

    mock_winterm_setting(setting);
    mock_stdpaths_locate("");
    auto mock_logger_guard = expect_log(lvl, "Could not find");

    mp::platform::sync_winterm_profiles();
}

INSTANTIATE_TEST_SUITE_P(PlatformWin, TestWinTermSyncLesserLogging,
                         Values(std::make_pair(QStringLiteral("none"), mpl::Level::debug),
                                std::make_pair(QStringLiteral("primary"), mpl::Level::warning)));

struct TestWinTermSyncModerateLogging : public TestWithParam<std::pair<QString, mpl::Level>>
{
};

TEST_P(TestWinTermSyncModerateLogging, logging_on_unreadable_settings)
{
    const auto& [setting, lvl] = GetParam();

    mock_winterm_setting(setting);
    mock_stdpaths_locate("C:\\unreadable\\profiles.json");
    auto mock_logger_guard = expect_log(lvl, "Could not read");

    mp::platform::sync_winterm_profiles();
}

TEST_P(TestWinTermSyncModerateLogging, logging_on_unparseable_settings)
{
    const auto& [setting, lvl] = GetParam();
    mock_winterm_setting(setting);

    auto json_file = fake_json("~!@#$% rubbish ^&*()_+");
    auto mock_logger_guard = expect_log(lvl, "Could not parse");

    mp::platform::sync_winterm_profiles();
}

INSTANTIATE_TEST_SUITE_P(PlatformWin, TestWinTermSyncModerateLogging,
                         Values(std::make_pair(QStringLiteral("none"), mpl::Level::info),
                                std::make_pair(QStringLiteral("primary"), mpl::Level::error)));

struct TestWinTermSyncJson : public TestWithParam<unsigned char>
{
    struct DressUpFlags
    {
        enum Value : unsigned char
        {
            None = 0,
            ProfilesDict = 1 << 0,
            ProfileBefore = 1 << 1,
            ProfileAfter = 1 << 2,
            CommentBefore = 1 << 3,
            CommentInline = 1 << 4,
            CommentAfter = 1 << 5,
            StuffOutside = 1 << 6,
            End = 1 << 7 // do not pass this - delimits the combination
        };
        static constexpr unsigned char begin = Value::None;
        static constexpr unsigned char end = Value::End;
    };

    void dress_with_comments(Json::Value& profiles, unsigned char flags)
    {
        if (flags & (DressUpFlags::CommentBefore | DressUpFlags::CommentInline | DressUpFlags::CommentAfter))
        {
            auto& elem = profiles.size() ? profiles[0] : profiles;
            const auto comment = std::string{"// a comment"};

            for (const auto [flag, place] : {std::make_pair(DressUpFlags::CommentBefore, Json::commentBefore),
                                             std::make_pair(DressUpFlags::CommentInline, Json::commentAfterOnSameLine),
                                             std::make_pair(DressUpFlags::CommentAfter, Json::commentAfter)})
                if (flags & flag)
                    elem.setComment(comment, place);
        }
    }

    void dress_with_extra_profiles(Json::Value& profiles, unsigned char flags)
    {
        if (flags & (DressUpFlags::ProfileBefore | DressUpFlags::ProfileAfter))
        {
            auto num_profiles = profiles.size();
            for (const auto [flag, distinctive] : {std::make_pair(DressUpFlags::ProfileBefore, "aaa"),
                                                   std::make_pair(DressUpFlags::ProfileAfter, "zzz")})
                if (flags & flag)
                {
                    profiles[num_profiles]["guid"] = fmt::format("fake_id_{}", distinctive);
                    profiles[num_profiles]["command"] = fmt::format("FAKEEEE {}", distinctive);

                    if (flag == DressUpFlags::ProfileBefore && num_profiles)
                        profiles[0u].swap(profiles[num_profiles]);

                    ++num_profiles;
                }
        }
    }

    void dress_with_dict(Json::Value& profiles, unsigned char flags)
    {
        if (flags & DressUpFlags::ProfilesDict)
        {
            Json::Value profiles_dict;
            profiles_dict["list"] = profiles;
            profiles_dict["defaults"]["var"] = "val";
            profiles_dict["defaults"]["foo"] = "bar";
            profiles.swap(profiles_dict);
        }
    }

    void dress_with_stuff(Json::Value& json, unsigned char flags)
    {
        if (flags & DressUpFlags::StuffOutside)
            json["stuff"]["a"]["b"]["c"] = "asdf";
    }

    void dress_up(Json::Value& json, unsigned char flags)
    {
        // std::cout << "DEBUG json before: " << json << std::endl;
        auto& profiles = json["profiles"];
        ASSERT_LE(profiles.size(), 1u);

        dress_with_comments(profiles, flags);
        dress_with_extra_profiles(profiles, flags);
        dress_with_dict(profiles, flags);
        dress_with_stuff(json, flags);
        // std::cout << "DEBUG json after: " << json << std::endl;
    }

    Json::Value& setup_primary_profile(Json::Value& json)
    {
        auto& ret = json["profiles"][0u];
        ret["guid"] = mp::winterm_profile_guid;
        return ret;
    }

    const Json::Value& get_profiles(const Json::Value& json)
    {
        const auto& profiles = json["profiles"];
        if (profiles.isNull() || profiles.isArray() || !profiles.isMember("list"))
            return profiles;
        return profiles["list"];
    }

    Json::Value& edit_profiles(Json::Value& json)
    {
        return const_cast<Json::Value&>(get_profiles(json));
    }

    const Json::Value& get_primary_profile(const Json::Value& json)
    {
        const auto& profiles = get_profiles(json);

        auto it = std::find_if(profiles.begin(), profiles.end(),
                               [](const auto& profile) { return profile["guid"] == mp::winterm_profile_guid; });

        if (it == profiles.end())
            throw std::runtime_error{"Test error - could not find primary profile"};

        return *it;
    }

    Json::Value& edit_primary_profile(Json::Value& json)
    {
        return const_cast<Json::Value&>(get_primary_profile(json));
    }
};

TEST_P(TestWinTermSyncJson, winterm_sync_keeps_visible_profile_if_setting_primary)
{
    mock_winterm_setting("primary");
    const auto guarded_logger = guarded_mock_logger(); // strict mock expects no calls

    Json::Value json;
    auto& profile = setup_primary_profile(json);
    profile["hidden"] = false;

    dress_up(json, GetParam());
    const auto json_file = fake_json(json);

    mp::platform::sync_winterm_profiles();

    EXPECT_EQ(json, read_json(json_file->fileName()));
}

TEST_P(TestWinTermSyncJson, winterm_sync_enables_hidden_profile_if_setting_primary)
{
    mock_winterm_setting("primary");
    const auto guarded_logger = guarded_mock_logger(); // strict mock expects no calls

    Json::Value json;
    auto& profile = setup_primary_profile(json);
    profile["hidden"] = true;

    dress_up(json, GetParam());
    const auto json_file = fake_json(json);

    mp::platform::sync_winterm_profiles();

    edit_primary_profile(json)["hidden"] = false;
    EXPECT_EQ(json, read_json(json_file->fileName()));
}

TEST_P(TestWinTermSyncJson, winterm_sync_keeps_profile_without_hidden_flag_if_setting_primary)
{
    mock_winterm_setting("primary");
    const auto guarded_logger = guarded_mock_logger(); // strict mock expects no calls

    Json::Value json;
    setup_primary_profile(json);

    dress_up(json, GetParam());
    const auto json_file = fake_json(json);

    mp::platform::sync_winterm_profiles();

    EXPECT_EQ(json, read_json(json_file->fileName()));
}

TEST_P(TestWinTermSyncJson, winterm_sync_adds_missing_profile_if_setting_primary)
{
    mock_winterm_setting("primary");
    const auto guarded_logger = guarded_mock_logger(); // strict mock expects no calls

    Json::Value json_in;
    dress_up(json_in, GetParam());
    const auto json_file = fake_json(json_in);

    mp::platform::sync_winterm_profiles();
    const auto json_out = read_json(json_file->fileName());

    Json::Value primary_profile;
    ASSERT_NO_THROW(primary_profile = get_primary_profile(json_out));
    EXPECT_EQ(primary_profile["name"], "Multipass");
    EXPECT_THAT(primary_profile["fontFace"].asString(), HasSubstr("Ubuntu"));
    EXPECT_THAT(primary_profile["icon"].asString(), EndsWith(".ico"));
    EXPECT_TRUE(primary_profile.isMember("background"));

    auto json_proof = json_out; // copy json_out so we can keep it const (to ensure indexing doesn't change it above)
    edit_profiles(json_proof) = get_profiles(json_in);
    EXPECT_EQ(json_proof, json_in); // confirm the rest of the json is the same
}

TEST_P(TestWinTermSyncJson, winterm_sync_keeps_missing_profile_if_setting_none)
{
    mock_winterm_setting("none");
    const auto guarded_logger = guarded_mock_logger();

    Json::Value json;
    dress_up(json, GetParam());
    const auto json_file = fake_json(json);

    mp::platform::sync_winterm_profiles();
    EXPECT_EQ(json, read_json(json_file->fileName()));
}

TEST_P(TestWinTermSyncJson, winterm_sync_keeps_hidden_profile_if_setting_none)
{
    mock_winterm_setting("none");
    const auto guarded_logger = guarded_mock_logger(); // strict mock expects no calls

    Json::Value json;
    auto& profile = setup_primary_profile(json);
    profile["hidden"] = true;

    dress_up(json, GetParam());
    const auto json_file = fake_json(json);

    mp::platform::sync_winterm_profiles();
    EXPECT_EQ(json, read_json(json_file->fileName()));
}

TEST_P(TestWinTermSyncJson, winterm_sync_disables_visible_profile_if_setting_none)
{
    mock_winterm_setting("none");
    const auto guarded_logger = guarded_mock_logger(); // strict mock expects no calls

    Json::Value json;
    auto& profile = setup_primary_profile(json);
    profile["hidden"] = false;

    dress_up(json, GetParam());
    const auto json_file = fake_json(json);

    mp::platform::sync_winterm_profiles();

    edit_primary_profile(json)["hidden"] = true;
    EXPECT_EQ(json, read_json(json_file->fileName()));
}

TEST_P(TestWinTermSyncJson, winterm_sync_disables_profile_without_hidden_flag_if_setting_none)
{
    mock_winterm_setting("none");
    const auto guarded_logger = guarded_mock_logger(); // strict mock expects no calls

    Json::Value json;
    setup_primary_profile(json);

    dress_up(json, GetParam());
    const auto json_file = fake_json(json);

    mp::platform::sync_winterm_profiles();

    edit_primary_profile(json)["hidden"] = true;
    EXPECT_EQ(json, read_json(json_file->fileName()));
}

INSTANTIATE_TEST_SUITE_P(PlatformWin, TestWinTermSyncJson,
                         Range(TestWinTermSyncJson::DressUpFlags::begin, TestWinTermSyncJson::DressUpFlags::end));

/*
 * TODO@ricab other cases to test
 *   - json empty
 *   - json empty dict
 *   - profiles list empty
 *   - profiles list w/ stuff
 *   - profiles array empty
 *   - profiles array w/ stuff
 */

} // namespace
