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

#include "tests/common.h"
#include "tests/mock_environment_helpers.h"
#include "tests/mock_file_ops.h"
#include "tests/mock_logger.h"
#include "tests/mock_settings.h"
#include "tests/mock_standard_paths.h"
#include "tests/mock_utils.h"
#include "tests/temp_dir.h"

#include <src/platform/platform_proprietary.h>

#include <multipass/constants.h>
#include <multipass/exceptions/settings_exceptions.h>
#include <multipass/platform.h>
#include <multipass/utils.h>

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QTemporaryFile>

#include <json/json.h>

#include <scope_guard.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <utility>

namespace mp = multipass;
namespace mpl = mp::logging;
namespace mpt = mp::test;
using namespace testing;

namespace
{

auto expect_only_log(multipass::logging::Level lvl, const std::string& substr)
{
    auto logger_scope = mpt::MockLogger::inject();
    logger_scope.mock_logger->screen_logs();
    logger_scope.mock_logger->expect_log(lvl, substr);
    return logger_scope;
}

void mock_stdpaths_locate(const QString& ret)
{
    EXPECT_CALL(mpt::MockStandardPaths::mock_instance(),
                locate(_, Property(&QString::toStdString, EndsWith("settings.json")), _))
        .WillOnce(Return(ret));
}

auto guarded_fake_json(const char* contents)
{
    QTemporaryFile json_file;
    EXPECT_TRUE(json_file.open());   // can't use gtest's asserts in non-void function
    EXPECT_TRUE(json_file.exists()); // idem

    json_file.write(contents);
    json_file.close();

    auto filename = json_file.fileName();
    mock_stdpaths_locate(filename);

    json_file.setAutoRemove(false); // we need to release the file but keep it around, so that it can be overwritten
    auto guard =
        sg::make_scope_guard([filename = filename.toStdString()]() noexcept { // we have to remove it ourselves later on
            std::error_code ec;
            std::filesystem::remove(filename, ec);
        });

    return std::make_pair(filename, std::move(guard)); // needs to be moved into the pair first (NRVO does not apply)
}

// ptr works around uncopyable QTemporaryFile
auto guarded_fake_json(const Json::Value& json)
{
    std::ostringstream oss;
    oss << json;
    const auto data = oss.str();

    return guarded_fake_json(data.c_str());
}

Json::Value read_json(const QString& filename)
{
    std::ifstream ifs{filename.toStdString(), std::ifstream::binary};
    EXPECT_TRUE(ifs); // can't use gtest's asserts in non-void function

    Json::Value json;
    EXPECT_NO_THROW(ifs >> json); // idem

    return json;
}

Json::Value& setup_primary_profile(Json::Value& json)
{
    auto& ret = json["profiles"][0u];
    ret["guid"] = mp::winterm_profile_guid;
    return ret;
}

TEST(PlatformWin, test_interpretation_of_unknown_settings_not_supported)
{
    for (const auto k : {"unimaginable", "katxama", "katxatxa"})
        for (const auto v : {"no", "matter", "what"})
            EXPECT_THROW(mp::platform::interpret_setting(k, v), mp::InvalidSettingException);
}

TEST(PlatformWin, winterm_in_extra_client_settings)
{
    auto extras = MP_PLATFORM.extra_client_settings();
    ASSERT_EQ(extras.size(), 1);

    auto& spec = **extras.begin();
    MP_EXPECT_THROW_THAT(spec.interpret("wrong"), mp::InvalidSettingException,
                         mpt::match_what(HasSubstr(mp::winterm_key)));
}

TEST(PlatformWin, no_extra_daemon_settings)
{
    EXPECT_THAT(MP_PLATFORM.extra_daemon_settings(), IsEmpty());
}

TEST(PlatformWin, test_default_driver)
{
    EXPECT_THAT(MP_PLATFORM.default_driver(), AnyOf("hyperv", "virtualbox"));
}

TEST(PlatformWin, test_default_privileged_mounts)
{
    EXPECT_EQ(MP_PLATFORM.default_privileged_mounts(), "false");
}

TEST(PlatformWin, valid_winterm_setting_values)
{
    for (const auto x : {"none", "primary"})
        EXPECT_EQ(mp::platform::interpret_setting(mp::winterm_key, x), x);
}

TEST(PlatformWin, winterm_setting_values_case_insensitive)
{
    for (const auto x : {"NoNe", "NONE", "nonE", "NonE"})
        EXPECT_EQ(mp::platform::interpret_setting(mp::winterm_key, x), "none");

    for (const auto x : {"pRIMARY", "Primary", "pRimarY"})
        EXPECT_EQ(mp::platform::interpret_setting(mp::winterm_key, x), "primary");
}

TEST(PlatformWin, unsupported_winterm_setting_values_cause_exception)
{
    for (const auto x : {"Unsupported", "values", "1", "000", "false", "True", "", "  "})
        MP_EXPECT_THROW_THAT(
            mp::platform::interpret_setting(mp::winterm_key, x), mp::InvalidSettingException,
            Property(&mp::InvalidSettingException::what,
                     AllOf(HasSubstr(mp::winterm_key), HasSubstr(x), HasSubstr("none"), HasSubstr("primary"))));
}

TEST(PlatformWin, blueprintsURLOverrideSetUnlockSetReturnsExpectedData)
{
    const QString fake_url{"https://a.fake.url"};
    mpt::SetEnvScope blueprints_url("MULTIPASS_BLUEPRINTS_URL", fake_url.toUtf8());
    mpt::SetEnvScope unlock{"MULTIPASS_UNLOCK", mp::platform::unlock_code};

    EXPECT_EQ(MP_PLATFORM.get_blueprints_url_override(), fake_url);
}

TEST(PlatformWin, blueprintsURLOverrideSetUnlockiNotSetReturnsEmptyString)
{
    const QString fake_url{"https://a.fake.url"};
    mpt::SetEnvScope blueprints_url("MULTIPASS_BLUEPRINTS_URL", fake_url.toUtf8());

    EXPECT_TRUE(MP_PLATFORM.get_blueprints_url_override().isEmpty());
}

TEST(PlatformWin, blueprintsURLOverrideNotSetReturnsEmptyString)
{
    EXPECT_TRUE(MP_PLATFORM.get_blueprints_url_override().isEmpty());
}

struct TestWinTermBase : public Test
{
    void mock_winterm_setting(const QString& ret)
    {
        EXPECT_CALL(mock_settings, get(Eq(mp::winterm_key))).WillOnce(Return(ret));
    }

    mpt::MockSettings::GuardedMock mock_settings_injection = mpt::MockSettings::inject<StrictMock>();
    mpt::MockSettings& mock_settings = *mock_settings_injection.first;
};

struct TestWinTermSyncLesserLogging : public TestWinTermBase, public WithParamInterface<std::pair<QString, mpl::Level>>
{
};

TEST_P(TestWinTermSyncLesserLogging, logging_on_no_file)
{
    const auto& [setting, lvl] = GetParam();

    mock_winterm_setting(setting);
    mock_stdpaths_locate("");
    auto mock_logger_guard = expect_only_log(lvl, "Could not find");

    mp::platform::sync_winterm_profiles();
}

INSTANTIATE_TEST_SUITE_P(PlatformWin, TestWinTermSyncLesserLogging,
                         Values(std::make_pair(QStringLiteral("none"), mpl::Level::debug),
                                std::make_pair(QStringLiteral("primary"), mpl::Level::warning)));

struct TestWinTermSyncModerateLogging : public TestWinTermBase,
                                        public WithParamInterface<std::pair<QString, mpl::Level>>
{
};

TEST_P(TestWinTermSyncModerateLogging, logging_on_unreadable_settings)
{
    const auto& [setting, lvl] = GetParam();

    mock_winterm_setting(setting);
    mock_stdpaths_locate("C:\\unreadable\\settings.json");
    const auto mock_logger_guard = expect_only_log(lvl, "Could not read");

    mp::platform::sync_winterm_profiles();
}

TEST_P(TestWinTermSyncModerateLogging, logging_on_unparseable_settings)
{
    const auto& [setting, lvl] = GetParam();
    mock_winterm_setting(setting);

    const auto [json_file_name, tmp_file_guard] = guarded_fake_json("~!@#$% rubbish ^&*()_+");
    const auto mock_logger_guard = expect_only_log(lvl, "Could not parse");

    mp::platform::sync_winterm_profiles();
}

TEST_P(TestWinTermSyncModerateLogging, logging_on_unavailable_profiles)
{
    const auto& [setting, lvl] = GetParam();
    mock_winterm_setting(setting);

    const auto [json_file_name, tmp_file_guard] = guarded_fake_json("{ \"someNode\": \"someValue\" }");
    const auto mock_logger_guard = expect_only_log(lvl, "Could not find");

    mp::platform::sync_winterm_profiles();
}

INSTANTIATE_TEST_SUITE_P(PlatformWin, TestWinTermSyncModerateLogging,
                         Values(std::make_pair(QStringLiteral("none"), mpl::Level::info),
                                std::make_pair(QStringLiteral("primary"), mpl::Level::error)));

struct TestWinTermSyncGreaterLogging : public TestWinTermBase, public WithParamInterface<QString>
{
};

TEST_P(TestWinTermSyncGreaterLogging, logging_on_failure_to_overwrite)
{
    const auto& setting = GetParam();
    mock_winterm_setting(setting);

    Json::Value json;
    auto& profile = setup_primary_profile(json);
    profile["hidden"] = (setting != "none"); // make this the opposite of what the setting says, to require an update

    const auto [json_file_name, tmp_file_guard] = guarded_fake_json(json);

    std::ifstream handle{json_file_name.toStdString()}; // open the file, to provoke a failure in overwriting
    const auto mock_logger_guard = expect_only_log(mpl::Level::error, "Could not update");

    mp::platform::sync_winterm_profiles();
}

INSTANTIATE_TEST_SUITE_P(PlatformWin, TestWinTermSyncGreaterLogging,
                         Values(QStringLiteral("none"), QStringLiteral("primary")));

struct TestWinTermSyncNoLeftovers : public TestWinTermBase, public WithParamInterface<bool>
{
};

TEST_P(TestWinTermSyncNoLeftovers, no_leftover_files_on_overwriting)
{
    bool fail = GetParam();
    mock_winterm_setting("primary");

    Json::Value json;
    json["profiles"];
    const auto [json_file_name, tmp_file_guard] = guarded_fake_json(json);

    const auto json_file_info = QFileInfo{json_file_name};
    const auto file_list = json_file_info.dir().entryList();

    std::ifstream handle;
    auto logger_scope = mpt::MockLogger::inject();
    logger_scope.mock_logger->screen_logs();

    if (fail)
    {
        handle.open(json_file_name.toStdString()); // block overwriting
        EXPECT_CALL(*logger_scope.mock_logger, log(mpl::Level::error, _, _));
    }

    mp::platform::sync_winterm_profiles();

    if (fail)
        ASSERT_EQ(read_json(json_file_name), json);
    else
        ASSERT_NE(read_json(json_file_name), json);

    EXPECT_EQ(json_file_info.dir().entryList(), file_list);
}

INSTANTIATE_TEST_SUITE_P(PlatformWin, TestWinTermSyncNoLeftovers, Values(true, false));

class TestWinTermSyncJson : public TestWinTermBase, public WithParamInterface<unsigned char>
{
public:
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

    void SetUp() override
    {
        logger_scope.mock_logger->screen_logs(mpl::Level::warning);
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

private:
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

    mpt::MockLogger::Scope logger_scope = mpt::MockLogger::inject();
};

TEST_P(TestWinTermSyncJson, winterm_sync_keeps_visible_profile_if_setting_primary)
{
    mock_winterm_setting("primary");

    Json::Value json;
    auto& profile = setup_primary_profile(json);
    profile["hidden"] = false;

    dress_up(json, GetParam());
    const auto [json_file_name, tmp_file_guard] = guarded_fake_json(json);
    const auto timestamp = QFileInfo{json_file_name}.lastModified();

    mp::platform::sync_winterm_profiles();

    EXPECT_EQ(QFileInfo{json_file_name}.lastModified(), timestamp);
    EXPECT_EQ(json, read_json(json_file_name));
}

TEST_P(TestWinTermSyncJson, winterm_sync_enables_hidden_profile_if_setting_primary)
{
    mock_winterm_setting("primary");

    Json::Value json;
    auto& profile = setup_primary_profile(json);
    profile["hidden"] = true;

    dress_up(json, GetParam());
    const auto [json_file_name, tmp_file_guard] = guarded_fake_json(json);

    mp::platform::sync_winterm_profiles();

    edit_primary_profile(json)["hidden"] = false;
    EXPECT_EQ(json, read_json(json_file_name));
}

TEST_P(TestWinTermSyncJson, winterm_sync_keeps_profile_without_hidden_flag_if_setting_primary)
{
    mock_winterm_setting("primary");

    Json::Value json;
    setup_primary_profile(json);

    dress_up(json, GetParam());
    const auto [json_file_name, tmp_file_guard] = guarded_fake_json(json);
    const auto timestamp = QFileInfo{json_file_name}.lastModified();

    mp::platform::sync_winterm_profiles();

    EXPECT_EQ(QFileInfo{json_file_name}.lastModified(), timestamp);
    EXPECT_EQ(json, read_json(json_file_name));
}

TEST_P(TestWinTermSyncJson, winterm_sync_adds_missing_profile_if_setting_primary)
{
    mock_winterm_setting("primary");

    Json::Value json_in;
    dress_up(json_in, GetParam());
    const auto [json_file_name, tmp_file_guard] = guarded_fake_json(json_in);

    mp::platform::sync_winterm_profiles();
    const auto json_out = read_json(json_file_name);

    Json::Value primary_profile;
    ASSERT_NO_THROW(primary_profile = get_primary_profile(json_out));
    EXPECT_EQ(primary_profile["name"], "Multipass");
    EXPECT_THAT(primary_profile["commandline"].asString(), HasSubstr(mp::client_name));
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

    Json::Value json;
    dress_up(json, GetParam());
    const auto [json_file_name, tmp_file_guard] = guarded_fake_json(json);
    const auto timestamp = QFileInfo{json_file_name}.lastModified();

    mp::platform::sync_winterm_profiles();

    EXPECT_EQ(QFileInfo{json_file_name}.lastModified(), timestamp);
    EXPECT_EQ(json, read_json(json_file_name));
}

TEST_P(TestWinTermSyncJson, winterm_sync_keeps_hidden_profile_if_setting_none)
{
    mock_winterm_setting("none");

    Json::Value json;
    auto& profile = setup_primary_profile(json);
    profile["hidden"] = true;

    dress_up(json, GetParam());
    const auto [json_file_name, tmp_file_guard] = guarded_fake_json(json);
    const auto timestamp = QFileInfo{json_file_name}.lastModified();

    mp::platform::sync_winterm_profiles();

    EXPECT_EQ(QFileInfo{json_file_name}.lastModified(), timestamp);
    EXPECT_EQ(json, read_json(json_file_name));
}

TEST_P(TestWinTermSyncJson, winterm_sync_disables_visible_profile_if_setting_none)
{
    mock_winterm_setting("none");

    Json::Value json;
    auto& profile = setup_primary_profile(json);
    profile["hidden"] = false;

    dress_up(json, GetParam());
    const auto [json_file_name, tmp_file_guard] = guarded_fake_json(json);

    mp::platform::sync_winterm_profiles();

    edit_primary_profile(json)["hidden"] = true;
    EXPECT_EQ(json, read_json(json_file_name));
}

TEST_P(TestWinTermSyncJson, winterm_sync_disables_profile_without_hidden_flag_if_setting_none)
{
    mock_winterm_setting("none");

    Json::Value json;
    setup_primary_profile(json);

    dress_up(json, GetParam());
    const auto [json_file_name, tmp_file_guard] = guarded_fake_json(json);

    mp::platform::sync_winterm_profiles();

    edit_primary_profile(json)["hidden"] = true;
    EXPECT_EQ(json, read_json(json_file_name));
}

INSTANTIATE_TEST_SUITE_P(PlatformWin, TestWinTermSyncJson,
                         Range(TestWinTermSyncJson::DressUpFlags::begin, TestWinTermSyncJson::DressUpFlags::end));

TEST(PlatformWin, create_alias_script_works)
{
    const mpt::TempDir tmp_dir;

    EXPECT_CALL(mpt::MockStandardPaths::mock_instance(), writableLocation(mp::StandardPaths::HomeLocation))
        .WillOnce(Return(tmp_dir.path()));

    EXPECT_NO_THROW(MP_PLATFORM.create_alias_script("alias_name", mp::AliasDefinition{"instance", "command"}));

    QFile checked_script(tmp_dir.path() + "/AppData/local/multipass/bin/alias_name.bat");
    checked_script.open(QFile::ReadOnly);

    std::string script_line = checked_script.readLine().toStdString();
    EXPECT_THAT(script_line, HasSubstr("@"));
    EXPECT_THAT(script_line, HasSubstr(" alias_name -- %*\n"));
    EXPECT_TRUE(checked_script.atEnd());
}

TEST(PlatformWin, create_alias_script_overwrites)
{
    auto [mock_utils, guard1] = mpt::MockUtils::inject();
    auto [mock_file_ops, guard2] = mpt::MockFileOps::inject();

    EXPECT_CALL(*mock_utils, make_file_with_content(_, _, true)).Times(1);

    EXPECT_NO_THROW(MP_PLATFORM.create_alias_script("alias_name", mp::AliasDefinition{"instance", "other_command"}));
}

TEST(PlatformWin, create_alias_script_throws_if_cannot_create_path)
{
    auto [mock_file_ops, guard] = mpt::MockFileOps::inject();

    EXPECT_CALL(*mock_file_ops, mkpath(_, _)).WillOnce(Return(false));

    MP_EXPECT_THROW_THAT(MP_PLATFORM.create_alias_script("alias_name", mp::AliasDefinition{"instance", "command"}),
                         std::runtime_error, mpt::match_what(HasSubstr("failed to create dir '")));
}

TEST(PlatformWin, create_alias_script_throws_if_cannot_write_script)
{
    auto [mock_file_ops, guard] = mpt::MockFileOps::inject();

    EXPECT_CALL(*mock_file_ops, mkpath(_, _)).WillOnce(Return(true));
    EXPECT_CALL(*mock_file_ops, open(_, _)).WillOnce(Return(true));
    EXPECT_CALL(*mock_file_ops, write(_, _, _)).WillOnce(Return(747));

    MP_EXPECT_THROW_THAT(MP_PLATFORM.create_alias_script("alias_name", mp::AliasDefinition{"instance", "command"}),
                         std::runtime_error, mpt::match_what(HasSubstr("failed to write to file '")));
}

TEST(PlatformWin, remove_alias_script_works)
{
    const mpt::TempDir tmp_dir;
    QFile script_file(tmp_dir.path() + "/AppData/local/multipass/bin/alias_name.bat");

    EXPECT_CALL(mpt::MockStandardPaths::mock_instance(), writableLocation(mp::StandardPaths::HomeLocation))
        .WillOnce(Return(tmp_dir.path()));

    MP_UTILS.make_file_with_content(script_file.fileName().toStdString(), "script content\n");

    EXPECT_NO_THROW(MP_PLATFORM.remove_alias_script("alias_name"));

    EXPECT_FALSE(script_file.exists());
}

TEST(PlatformWin, remove_alias_script_throws_if_cannot_remove_script)
{
    const mpt::TempDir tmp_dir;
    QFile script_file(tmp_dir.path() + "/AppData/local/multipass/bin/alias_name.bat");

    EXPECT_CALL(mpt::MockStandardPaths::mock_instance(), writableLocation(mp::StandardPaths::HomeLocation))
        .WillOnce(Return(tmp_dir.path()));

    MP_EXPECT_THROW_THAT(MP_PLATFORM.remove_alias_script("alias_name"), std::runtime_error,
                         mpt::match_what(StrEq("error removing alias script")));
}
} // namespace
