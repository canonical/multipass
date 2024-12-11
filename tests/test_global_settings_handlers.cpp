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

#include "common.h"
#include "mock_platform.h"
#include "mock_qsettings.h"
#include "mock_settings.h"
#include "mock_standard_paths.h"
#include "mock_utils.h"

#include <multipass/cli/client_common.h>
#include <multipass/constants.h>
#include <multipass/settings/basic_setting_spec.h>
#include <multipass/settings/persistent_settings_handler.h>

#include <src/daemon/daemon_init_settings.h>

#include <QDir>

namespace mp = multipass;
namespace mpt = mp::test;
using namespace testing;

namespace
{
struct TestGlobalSettingsHandlers : public Test
{
    void SetUp() override
    {
        ON_CALL(mock_platform, default_privileged_mounts).WillByDefault(Return("true"));
        ON_CALL(mock_platform, is_backend_supported).WillByDefault(Return(true));

        EXPECT_CALL(mock_settings,
                    register_handler(Pointer(WhenDynamicCastTo<const mp::PersistentSettingsHandler*>(NotNull()))))
            .WillOnce([this](auto uptr) {
                handler = std::move(uptr);
                return handler.get();
            });
    }

    void inject_mock_qsettings() // moves the mock, so call once only, after setting expectations
    {
        EXPECT_CALL(*mock_qsettings, fileName)
            .WillRepeatedly(Return(QDir::temp().absoluteFilePath("missing_file.conf")));
        EXPECT_CALL(*mock_qsettings_provider, make_wrapped_qsettings(_, Eq(QSettings::IniFormat)))
            .WillOnce(Return(ByMove(std::move(mock_qsettings))));
    }

    void inject_default_returning_mock_qsettings()
    {
        EXPECT_CALL(*mock_qsettings_provider, make_wrapped_qsettings)
            .WillRepeatedly(WithArg<0>(Invoke(make_default_returning_mock_qsettings)));
    }

    void expect_setting_values(const std::map<QString, QString>& setting_values)
    {
        for (const auto& [k, v] : setting_values)
        {
            EXPECT_EQ(handler->get(k), v);
        }
    }

    template <typename... Ts>
    void assert_unrecognized_keys(Ts... keys)
    {
        for (const char* key : {keys...})
        {
            MP_ASSERT_THROW_THAT(handler->get(key), mp::UnrecognizedSettingException, mpt::match_what(HasSubstr(key)));
        }
    }

    static std::unique_ptr<multipass::WrappedQSettings> make_default_returning_mock_qsettings(const QString& filename)
    {
        auto mock_qsettings = std::make_unique<NiceMock<mpt::MockQSettings>>();
        EXPECT_CALL(*mock_qsettings, value_impl).WillRepeatedly(ReturnArg<1>());
        EXPECT_CALL(*mock_qsettings, fileName).WillRepeatedly(Return(filename));

        return mock_qsettings;
    }

    static mp::SettingSpec::Set to_setting_set(const std::map<QString, QString>& setting_defaults)
    {
        mp::SettingSpec::Set ret;
        for (const auto& [k, v] : setting_defaults)
            ret.insert(std::make_unique<mp::BasicSettingSpec>(k, v));

        return ret;
    }

public:
    mpt::MockQSettingsProvider::GuardedMock mock_qsettings_injection =
        mpt::MockQSettingsProvider::inject<StrictMock>(); /* strict to ensure that, other than explicitly injected, no
                                                             QSettings are used */
    mpt::MockQSettingsProvider* mock_qsettings_provider = mock_qsettings_injection.first;

    std::unique_ptr<NiceMock<mpt::MockQSettings>> mock_qsettings = std::make_unique<NiceMock<mpt::MockQSettings>>();

    mpt::MockSettings::GuardedMock mock_settings_injection = mpt::MockSettings::inject<StrictMock>();
    mpt::MockSettings& mock_settings = *mock_settings_injection.first;

    mpt::MockPlatform::GuardedMock mock_platform_injection = mpt::MockPlatform::inject<NiceMock>();
    mpt::MockPlatform& mock_platform = *mock_platform_injection.first;

    std::unique_ptr<mp::SettingsHandler> handler = nullptr;
};

TEST_F(TestGlobalSettingsHandlers, clientsRegisterPersistentHandlerWithClientFilename)
{
    auto config_location = QStringLiteral("/a/b/c");
    auto expected_filename = config_location + "/multipass/multipass.conf";

    EXPECT_CALL(mpt::MockStandardPaths::mock_instance(), writableLocation(mp::StandardPaths::GenericConfigLocation))
        .WillOnce(Return(config_location));

    mp::client::register_global_settings_handlers();

    EXPECT_CALL(*mock_qsettings_provider, make_wrapped_qsettings(Eq(expected_filename), _))
        .WillOnce(WithArg<0>(Invoke(make_default_returning_mock_qsettings)));
    handler->set(mp::petenv_key, "goo");
}

TEST_F(TestGlobalSettingsHandlers, clientsRegisterPersistentHandlerForClientSettings)
{
    mp::client::register_global_settings_handlers();

    inject_default_returning_mock_qsettings();

    expect_setting_values({{mp::petenv_key, "primary"}});
}

TEST_F(TestGlobalSettingsHandlers, clientsRegisterPersistentHandlerWithOverridingPlatformSettings)
{
    const auto platform_defaults = std::map<QString, QString>{{"client.a.setting", "a reasonably long value for this"},
                                                              {mp::petenv_key, "secondary"},
                                                              {"client.empty.setting", ""},
                                                              {"client.an.int", "-12345"},
                                                              {"client.a.float.with.a.long_key", "3.14"}};

    EXPECT_CALL(mock_platform, extra_client_settings).WillOnce(Return(ByMove(to_setting_set(platform_defaults))));
    mp::client::register_global_settings_handlers();
    inject_default_returning_mock_qsettings();

    expect_setting_values(platform_defaults);
}

TEST_F(TestGlobalSettingsHandlers, clientsDoNotRegisterPersistentHandlerForDaemonSettings)
{
    mp::client::register_global_settings_handlers();

    EXPECT_CALL(*mock_qsettings_provider, make_wrapped_qsettings(_, _)).Times(0);
    assert_unrecognized_keys(mp::driver_key, mp::bridged_interface_key, mp::mounts_key, mp::passphrase_key);
}

struct TestGoodPetEnvSetting : public TestGlobalSettingsHandlers, WithParamInterface<const char*>
{
};

TEST_P(TestGoodPetEnvSetting, clientsRegisterHandlerThatAcceptsValidPetenv)
{
    auto key = mp::petenv_key, val = GetParam();
    mp::client::register_global_settings_handlers();

    EXPECT_CALL(*mock_qsettings, setValue(Eq(key), Eq(val)));
    inject_mock_qsettings();

    ASSERT_NO_THROW(handler->set(key, val));
}

INSTANTIATE_TEST_SUITE_P(TestGoodPetEnvSetting, TestGoodPetEnvSetting, Values("valid-primary", ""));

struct TestBadPetEnvSetting : public TestGlobalSettingsHandlers, WithParamInterface<const char*>
{
};

TEST_P(TestBadPetEnvSetting, clientsRegisterHandlerThatRejectsInvalidPetenv)
{
    auto key = mp::petenv_key, val = GetParam();
    mp::client::register_global_settings_handlers();

    MP_ASSERT_THROW_THAT(handler->set(key, val),
                         mp::InvalidSettingException,
                         mpt::match_what(AllOf(HasSubstr(key), HasSubstr(val))));
}

INSTANTIATE_TEST_SUITE_P(TestBadPetEnvSetting, TestBadPetEnvSetting, Values("-", "-a-b-", "_asd", "_1", "1-2-3"));

TEST_F(TestGlobalSettingsHandlers, daemonRegistersPersistentHandlerWithDaemonFilename)
{
    auto config_location = QStringLiteral("/a/b/c");
    auto expected_filename = config_location + "/multipassd.conf";

    EXPECT_CALL(mock_platform, daemon_config_home).WillOnce(Return(config_location));

    mp::daemon::register_global_settings_handlers();

    EXPECT_CALL(*mock_qsettings_provider, make_wrapped_qsettings(Eq(expected_filename), _))
        .WillOnce(WithArg<0>(Invoke(make_default_returning_mock_qsettings)));
    handler->set(mp::bridged_interface_key, "bridge");
}

TEST_F(TestGlobalSettingsHandlers, daemonRegistersPersistentHandlerForDaemonSettings)
{
    const auto driver = "conductor";
    const auto mount = "false";

    EXPECT_CALL(mock_platform, default_driver).WillOnce(Return(driver));
    EXPECT_CALL(mock_platform, default_privileged_mounts).WillOnce(Return(mount));

    mp::daemon::register_global_settings_handlers();
    inject_default_returning_mock_qsettings();

    expect_setting_values({{mp::driver_key, driver}, {mp::bridged_interface_key, ""}, {mp::mounts_key, mount}});
}

TEST_F(TestGlobalSettingsHandlers, daemonRegistersPersistentHandlerForDaemonPlatformSettings)
{
    const auto platform_defaults = std::map<QString, QString>{{"local.blah", "blargh"},
                                                              {mp::driver_key, "platform-hypervisor"},
                                                              {"local.a.bool", "false"},
                                                              {mp::bridged_interface_key, "platform-bridge"},
                                                              {"local.foo", "barrrr"},
                                                              {mp::mounts_key, "false"},
                                                              {"local.a.long.number", "1234567890"}};

    EXPECT_CALL(mock_platform, default_driver).WillOnce(Return("unused"));
    EXPECT_CALL(mock_platform, default_privileged_mounts).WillOnce(Return("true"));
    EXPECT_CALL(mock_platform, extra_daemon_settings).WillOnce(Return(ByMove(to_setting_set(platform_defaults))));

    mp::daemon::register_global_settings_handlers();
    inject_default_returning_mock_qsettings();

    expect_setting_values(platform_defaults);
}

TEST_F(TestGlobalSettingsHandlers, daemonDoesNotRegisterPersistentHandlerForClientSettings)
{
    mp::daemon::register_global_settings_handlers();

    EXPECT_CALL(*mock_qsettings_provider, make_wrapped_qsettings(_, _)).Times(0);
    assert_unrecognized_keys(mp::petenv_key, mp::winterm_key);
}

TEST_F(TestGlobalSettingsHandlers, daemonRegistersHandlerThatAcceptsValidBackend)
{
    auto key = mp::driver_key, val = "good driver";

    mp::daemon::register_global_settings_handlers();

    EXPECT_CALL(mock_platform, is_backend_supported(Eq(val))).WillOnce(Return(true));
    EXPECT_CALL(*mock_qsettings, setValue(Eq(key), Eq(val)));
    inject_mock_qsettings();

    ASSERT_NO_THROW(handler->set(key, val));
}

TEST_F(TestGlobalSettingsHandlers, daemonRegistersHandlerThatTransformsHyperVDriver)
{
    const auto key = mp::driver_key;
    const auto val = "hyper-v";
    const auto transformed_val = "hyperv";

    mp::daemon::register_global_settings_handlers();

    EXPECT_CALL(*mock_qsettings, setValue(Eq(key), Eq(transformed_val))).Times(1);
    inject_mock_qsettings();

    ASSERT_NO_THROW(handler->set(key, val));
}

TEST_F(TestGlobalSettingsHandlers, daemonRegistersHandlerThatTransformsVBoxDriver)
{
    const auto key = mp::driver_key;
    const auto val = "vbox";
    const auto transformed_val = "virtualbox";

    mp::daemon::register_global_settings_handlers();

    EXPECT_CALL(*mock_qsettings, setValue(Eq(key), Eq(transformed_val))).Times(1);
    inject_mock_qsettings();

    ASSERT_NO_THROW(handler->set(key, val));
}

TEST_F(TestGlobalSettingsHandlers, daemonRegistersHandlerThatRejectsInvalidBackend)
{
    auto key = mp::driver_key, val = "bad driver";

    mp::daemon::register_global_settings_handlers();

    EXPECT_CALL(mock_platform, is_backend_supported(Eq(val))).WillOnce(Return(false));

    MP_ASSERT_THROW_THAT(handler->set(key, val),
                         mp::InvalidSettingException,
                         mpt::match_what(AllOf(HasSubstr(key), HasSubstr(val))));
}

TEST_F(TestGlobalSettingsHandlers, daemonRegistersHandlerThatAcceptsBoolMounts)
{
    mp::daemon::register_global_settings_handlers();

    EXPECT_CALL(*mock_qsettings, setValue(Eq(mp::mounts_key), Eq("true")));
    inject_mock_qsettings();

    ASSERT_NO_THROW(handler->set(mp::mounts_key, "1"));
}

TEST_F(TestGlobalSettingsHandlers, daemonRegistersHandlerThatAcceptsBrigedInterface)
{
    const auto val = "bridge";

    mp::daemon::register_global_settings_handlers();

    EXPECT_CALL(*mock_qsettings, setValue(Eq(mp::bridged_interface_key), Eq(val)));
    inject_mock_qsettings();

    ASSERT_NO_THROW(handler->set(mp::bridged_interface_key, val));
}

TEST_F(TestGlobalSettingsHandlers, daemonRegistersHandlerThatHashesNonEmptyPassword)
{
    const auto val = "correct horse battery staple";
    const auto hash = "xkcd";

    auto [mock_utils, guard] = mpt::MockUtils::inject<StrictMock>();
    EXPECT_CALL(*mock_utils, generate_scrypt_hash_for(Eq(val))).WillOnce(Return(hash));

    mp::daemon::register_global_settings_handlers();

    EXPECT_CALL(*mock_qsettings, setValue(Eq(mp::passphrase_key), Eq(hash)));
    inject_mock_qsettings();

    ASSERT_NO_THROW(handler->set(mp::passphrase_key, val));
}

TEST_F(TestGlobalSettingsHandlers, daemonRegistersHandlerThatResetsHashWhenPasswordIsEmpty)
{
    const auto val = "";

    mp::daemon::register_global_settings_handlers();

    EXPECT_CALL(*mock_qsettings, setValue(Eq(mp::passphrase_key), Eq(val)));
    inject_mock_qsettings();

    ASSERT_NO_THROW(handler->set(mp::passphrase_key, val));
}

} // namespace
