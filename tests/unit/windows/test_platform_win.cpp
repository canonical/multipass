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

#include "tests/unit/common.h"
#include "tests/unit/mock_environment_helpers.h"
#include "tests/unit/mock_file_ops.h"
#include "tests/unit/mock_logger.h"
#include "tests/unit/mock_settings.h"
#include "tests/unit/mock_standard_paths.h"
#include "tests/unit/mock_utils.h"
#include "tests/unit/temp_dir.h"

#include <multipass/constants.h>
#include <multipass/exceptions/settings_exceptions.h>
#include <multipass/json_utils.h>
#include <multipass/platform.h>
#include <multipass/utils.h>

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QTemporaryFile>

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

TEST(PlatformWin, testInterpretationOfUnknownSettingsNotSupported)
{
    for (const auto k : {"unimaginable", "katxama", "katxatxa"})
        for (const auto v : {"no", "matter", "what"})
            EXPECT_THROW(mp::platform::interpret_setting(k, v), mp::InvalidSettingException);
}

TEST(PlatformWin, wintermInExtraClientSettings)
{
    auto extras = MP_PLATFORM.extra_client_settings();
    ASSERT_EQ(extras.size(), 1);

    auto& spec = **extras.begin();
    MP_EXPECT_THROW_THAT(spec.interpret("wrong"),
                         mp::InvalidSettingException,
                         mpt::match_what(HasSubstr(mp::winterm_key)));
}

TEST(PlatformWin, noExtraDaemonSettings)
{
    EXPECT_THAT(MP_PLATFORM.extra_daemon_settings(), IsEmpty());
}

TEST(PlatformWin, testDefaultDriver)
{
    EXPECT_THAT(MP_PLATFORM.default_driver(), AnyOf("hyperv", "hyperv_api", "virtualbox"));
}

TEST(PlatformWin, testDefaultPrivilegedMounts)
{
    EXPECT_EQ(MP_PLATFORM.default_privileged_mounts(), "false");
}

TEST(PlatformWin, validWintermSettingValues)
{
    for (const auto x : {"none", "primary"})
        EXPECT_EQ(mp::platform::interpret_setting(mp::winterm_key, x), x);
}

TEST(PlatformWin, wintermSettingValuesCaseInsensitive)
{
    for (const auto x : {"NoNe", "NONE", "nonE", "NonE"})
        EXPECT_EQ(mp::platform::interpret_setting(mp::winterm_key, x), "none");

    for (const auto x : {"pRIMARY", "Primary", "pRimarY"})
        EXPECT_EQ(mp::platform::interpret_setting(mp::winterm_key, x), "primary");
}

TEST(PlatformWin, unsupportedWintermSettingValuesCauseException)
{
    for (const auto x : {"Unsupported", "values", "1", "000", "false", "True", "", "  "})
        MP_EXPECT_THROW_THAT(mp::platform::interpret_setting(mp::winterm_key, x),
                             mp::InvalidSettingException,
                             Property(&mp::InvalidSettingException::what,
                                      AllOf(HasSubstr(mp::winterm_key),
                                            HasSubstr(x),
                                            HasSubstr("none"),
                                            HasSubstr("primary"))));
}

struct TestWintermSync : public Test
{
    mpt::MockSettings::GuardedMock mock_settings_injection =
        mpt::MockSettings::inject<StrictMock>();
    mpt::MockSettings& mock_settings = *mock_settings_injection.first;

    mpt::MockFileOps::GuardedMock mock_file_ops_injection = mpt::MockFileOps::inject<StrictMock>();
    mpt::MockFileOps& mock_file_ops = *mock_file_ops_injection.first;
};

TEST_F(TestWintermSync, createsNewFragmentFile)
{
    EXPECT_CALL(mock_settings, get(Eq(mp::winterm_key))).WillOnce(Return("primary"));

    EXPECT_CALL(mock_file_ops, exists(A<const QDir&>())).WillOnce(Return(false));
    EXPECT_CALL(mock_file_ops, write_transactionally)
        .WillOnce(WithArg<1>([](const QByteArrayView& data) {
            try
            {
                auto value = boost::json::parse({data.begin(), data.end()});
                EXPECT_EQ(value_to<std::string>(value.at_pointer("/profiles/0/updates")),
                          mp::winterm_old_profile_guid);
                EXPECT_EQ(value_to<std::string>(value.at_pointer("/profiles/1/guid")),
                          mp::winterm_profile_guid);
            }
            catch (const std::exception& e)
            {
                // Ensure that all exceptions thrown above trigger a test failure.
                FAIL() << e.what();
            }
        }));

    mp::platform::sync_winterm_profiles();
}

TEST_F(TestWintermSync, skipsWritingWhenAlreadyExists)
{
    EXPECT_CALL(mock_settings, get(Eq(mp::winterm_key))).WillOnce(Return("primary"));

    EXPECT_CALL(mock_file_ops, exists(A<const QDir&>())).WillOnce(Return(true));
    EXPECT_CALL(mock_file_ops, write_transactionally).Times(0);

    const auto mock_logger_guard = expect_only_log(mpl::Level::debug, "already exists");
    mp::platform::sync_winterm_profiles();
}

TEST_F(TestWintermSync, logsOnNoDirectory)
{
    EXPECT_CALL(mock_settings, get(Eq(mp::winterm_key))).WillOnce(Return("primary"));
    EXPECT_CALL(mpt::MockStandardPaths::mock_instance(), standardLocations(_))
        .WillOnce(Return(QStringList{}));

    const auto mock_logger_guard = expect_only_log(mpl::Level::warning, "Could not find");
    mp::platform::sync_winterm_profiles();
}

TEST_F(TestWintermSync, logsOnFailureToOverwrite)
{
    EXPECT_CALL(mock_settings, get(Eq(mp::winterm_key))).WillOnce(Return("primary"));

    EXPECT_CALL(mock_file_ops, exists(A<const QDir&>())).WillOnce(Return(false));
    EXPECT_CALL(mock_file_ops, write_transactionally)
        .WillOnce(Throw(std::runtime_error("intentional")));

    const auto mock_logger_guard = expect_only_log(mpl::Level::error, "Failed to write");
    mp::platform::sync_winterm_profiles();
}

TEST(PlatformWin, createAliasScriptWorks)
{
    const mpt::TempDir tmp_dir;

    EXPECT_CALL(mpt::MockStandardPaths::mock_instance(),
                writableLocation(mp::StandardPaths::HomeLocation))
        .WillOnce(Return(tmp_dir.path()));

    EXPECT_NO_THROW(
        MP_PLATFORM.create_alias_script("alias_name", mp::AliasDefinition{"instance", "command"}));

    QFile checked_script(tmp_dir.path() + "/AppData/local/multipass/bin/alias_name.bat");
    ASSERT_TRUE(checked_script.open(QFile::ReadOnly));

    std::string script_line = checked_script.readLine().toStdString();
    EXPECT_THAT(script_line, HasSubstr("@"));
    EXPECT_THAT(script_line, HasSubstr(" alias_name -- %*\n"));
    EXPECT_TRUE(checked_script.atEnd());
}

TEST(PlatformWin, createAliasScriptOverwrites)
{
    auto [mock_utils, guard1] = mpt::MockUtils::inject();
    auto [mock_file_ops, guard2] = mpt::MockFileOps::inject();

    EXPECT_CALL(*mock_utils, make_file_with_content(_, _, true)).Times(1);

    EXPECT_NO_THROW(
        MP_PLATFORM.create_alias_script("alias_name",
                                        mp::AliasDefinition{"instance", "other_command"}));
}

TEST(PlatformWin, createAliasScriptThrowsIfCannotCreatePath)
{
    auto [mock_file_ops, guard] = mpt::MockFileOps::inject();

    EXPECT_CALL(*mock_file_ops, mkpath(_, _)).WillOnce(Return(false));

    MP_EXPECT_THROW_THAT(
        MP_PLATFORM.create_alias_script("alias_name", mp::AliasDefinition{"instance", "command"}),
        std::runtime_error,
        mpt::match_what(HasSubstr("failed to create dir '")));
}

TEST(PlatformWin, createAliasScriptThrowsIfCannotWriteScript)
{
    auto [mock_file_ops, guard] = mpt::MockFileOps::inject();

    EXPECT_CALL(*mock_file_ops, mkpath(_, _)).WillOnce(Return(true));
    EXPECT_CALL(*mock_file_ops, open(_, _)).WillOnce(Return(true));
    EXPECT_CALL(*mock_file_ops, write(A<QIODevice&>(), _, _)).WillOnce(Return(747));

    MP_EXPECT_THROW_THAT(
        MP_PLATFORM.create_alias_script("alias_name", mp::AliasDefinition{"instance", "command"}),
        std::runtime_error,
        mpt::match_what(HasSubstr("failed to write to file '")));
}

TEST(PlatformWin, removeAliasScriptWorks)
{
    const mpt::TempDir tmp_dir;
    QFile script_file(tmp_dir.path() + "/AppData/local/multipass/bin/alias_name.bat");

    EXPECT_CALL(mpt::MockStandardPaths::mock_instance(),
                writableLocation(mp::StandardPaths::HomeLocation))
        .WillOnce(Return(tmp_dir.path()));

    MP_UTILS.make_file_with_content(script_file.fileName().toStdString(), "script content\n");

    EXPECT_NO_THROW(MP_PLATFORM.remove_alias_script("alias_name"));

    EXPECT_FALSE(script_file.exists());
}

TEST(PlatformWin, removeAliasScriptThrowsIfCannotRemoveScript)
{
    const mpt::TempDir tmp_dir;
    QFile script_file(tmp_dir.path() + "/AppData/local/multipass/bin/alias_name.bat");

    EXPECT_CALL(mpt::MockStandardPaths::mock_instance(),
                writableLocation(mp::StandardPaths::HomeLocation))
        .WillOnce(Return(tmp_dir.path()));

    MP_EXPECT_THROW_THAT(MP_PLATFORM.remove_alias_script("alias_name"),
                         std::runtime_error,
                         mpt::match_what(StrEq("error removing alias script")));
}

TEST(PlatformWin, getCpusReturnsGreaterThanZero)
{
    EXPECT_GT(MP_PLATFORM.get_cpus(), 0);
}

TEST(PlatformWin, getTotalRamReturnsGreaterThanZero)
{
    EXPECT_GT(MP_PLATFORM.get_total_ram(), 0LL);
}

TEST(PlatformWin, test_qstr_path_conversion)
{
    // ASCII
    QString ascii = QStringLiteral("/some/plain/path.txt");
    EXPECT_EQ(MP_PLATFORM.path_to_qstr(MP_PLATFORM.qstr_to_path(ascii)), ascii);

    // BMP Unicode (Turkish, CJK)
    QString bmp = QStringLiteral("/belgeler/İzmir/日本語.txt");
    EXPECT_EQ(MP_PLATFORM.path_to_qstr(MP_PLATFORM.qstr_to_path(bmp)), bmp);

    // Astral plane (emoji – surrogate pair in UTF-16)
    QString astral = QStringLiteral("/tmp/\U0001F600/file.txt");
    EXPECT_EQ(MP_PLATFORM.path_to_qstr(MP_PLATFORM.qstr_to_path(astral)), astral);

    // Empty
    EXPECT_EQ(MP_PLATFORM.qstr_to_path(QString{}), std::filesystem::path{});

    // Spaces and special filesystem characters
    QString special = QStringLiteral("/path with spaces/file (1).txt");
    EXPECT_EQ(MP_PLATFORM.path_to_qstr(MP_PLATFORM.qstr_to_path(special)), special);
}

} // namespace
