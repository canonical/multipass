/*
 * Copyright (C) 2021 Canonical, Ltd.
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

#include <multipass/cli/client_common.h>
#include <multipass/constants.h>
#include <multipass/settings/basic_setting_spec.h>
#include <multipass/settings/persistent_settings_handler.h>

#include <src/daemon/daemon_init_settings.h>

#include <QKeySequence>

namespace mp = multipass;
namespace mpt = mp::test;
using namespace testing;

namespace
{
struct TestRegisteredSettingsHandlers : public Test
{
    void SetUp() override
    {
        ON_CALL(mock_platform, default_privileged_mounts).WillByDefault(Return("true"));
        ON_CALL(mock_platform, is_backend_supported).WillByDefault(Return(true));
    }

    void inject_mock_qsettings() // moves the mock, so call once only, after setting expectations
    {
        EXPECT_CALL(*mock_qsettings_provider, make_wrapped_qsettings(_, Eq(QSettings::IniFormat)))
            .WillOnce(Return(ByMove(std::move(mock_qsettings))));
    }

    void inject_default_returning_mock_qsettings() // TODO@ricab do I really need this after all?
    {
        EXPECT_CALL(*mock_qsettings_provider, make_wrapped_qsettings)
            .WillRepeatedly(InvokeWithoutArgs(make_default_returning_mock_qsettings));
    }

    static std::unique_ptr<multipass::WrappedQSettings> make_default_returning_mock_qsettings()
    {
        auto mock_qsettings = std::make_unique<NiceMock<mpt::MockQSettings>>();
        EXPECT_CALL(*mock_qsettings, value_impl).WillRepeatedly(ReturnArg<1>());

        return mock_qsettings;
    }

    void grab_registered_persistent_handler(std::unique_ptr<mp::SettingsHandler>& handler)
    {
        auto grab_it = [&handler](auto&& arg) {
            handler = std::move(arg);
            EXPECT_THAT(dynamic_cast<mp::PersistentSettingsHandler*>(handler.get()), NotNull()); // TODO@ricab matcher
        };

        EXPECT_CALL(mock_settings, register_handler(NotNull())) // TODO@ricab will need to distinguish types, need #2282
            .WillOnce(grab_it)
            .WillRepeatedly(Return()) // TODO@ricab drop this when daemon handler is gone
            ;
    }

public:
    mpt::MockQSettingsProvider::GuardedMock mock_qsettings_injection =
        mpt::MockQSettingsProvider::inject<StrictMock>(); /* strict to ensure that, other than explicitly injected, no
                                                             QSettings are used */
    mpt::MockQSettingsProvider* mock_qsettings_provider = mock_qsettings_injection.first;

    std::unique_ptr<NiceMock<mpt::MockQSettings>> mock_qsettings = std::make_unique<NiceMock<mpt::MockQSettings>>();
    mpt::MockSettings& mock_settings = mpt::MockSettings::mock_instance();

    mpt::MockPlatform::GuardedMock mock_platform_injection = mpt::MockPlatform::inject<NiceMock>();
    mpt::MockPlatform& mock_platform = *mock_platform_injection.first;
};

// TODO@ricab de-duplicate daemon/client tests where possible (TEST_P)

TEST_F(TestRegisteredSettingsHandlers, clientsRegisterPersistentHandlerWithClientFilename)
{
    auto config_location = QStringLiteral("/a/b/c");
    auto expected_filename = config_location + "/multipass/multipass.conf";

    EXPECT_CALL(mpt::MockStandardPaths::mock_instance(), writableLocation(mp::StandardPaths::GenericConfigLocation))
        .WillOnce(Return(config_location));

    std::unique_ptr<mp::SettingsHandler> handler = nullptr;
    grab_registered_persistent_handler(handler);
    mp::client::register_settings_handlers();

    EXPECT_CALL(*mock_qsettings_provider, make_wrapped_qsettings(Eq(expected_filename), _))
        .WillOnce(InvokeWithoutArgs(make_default_returning_mock_qsettings));
    handler->set(mp::petenv_key, "goo");
}

TEST_F(TestRegisteredSettingsHandlers, clientsRegisterPersistentHandlerForClient_settings)
{
    std::unique_ptr<mp::SettingsHandler> handler = nullptr;
    grab_registered_persistent_handler(handler);
    mp::client::register_settings_handlers();

    inject_default_returning_mock_qsettings();
    EXPECT_EQ(handler->get(mp::petenv_key), "primary");
    EXPECT_EQ(handler->get(mp::autostart_key), "true");
    EXPECT_EQ(QKeySequence{handler->get(mp::hotkey_key)}, QKeySequence{mp::hotkey_default});
}

TEST_F(TestRegisteredSettingsHandlers, clientsRegisterPersistentHandlerForClientPlatformSettings)
{
    const auto client_defaults = std::map<QString, QString>{{"client.a.setting", "a reasonably long value for this"},
                                                            {"client.empty.setting", ""},
                                                            {"client.an.int", "-12345"},
                                                            {"client.a.float.with.a.long_key", "3.14"}};

    mp::SettingSpec::Set client_settings;
    for (const auto& [k, v] : client_defaults)
        client_settings.insert(std::make_unique<mp::BasicSettingSpec>(k, v));

    EXPECT_CALL(mock_platform, extra_client_settings).WillOnce(Return(ByMove(std::move(client_settings))));

    std::unique_ptr<mp::SettingsHandler> handler = nullptr;
    grab_registered_persistent_handler(handler);
    mp::client::register_settings_handlers();

    inject_default_returning_mock_qsettings();

    for (const auto& [k, v] : client_defaults)
    {
        EXPECT_EQ(handler->get(k), v);
    }
}

TEST_F(TestRegisteredSettingsHandlers, clientsRegisterPersistentHandlerWithOverriddenPlatformDefaults)
{
    // TODO@ricab
}

TEST_F(TestRegisteredSettingsHandlers, clientsDoNotRegisterPersistentHandlerForDaemonSettings)
{
    // TODO@ricab
}

TEST_F(TestRegisteredSettingsHandlers, clientsRegisterHandlerThatTranslatesHotkey)
{
    const auto key = mp::hotkey_key;
    const auto val = "Alt+X";
    const auto native_val = mp::platform::interpret_setting(key, val);

    std::unique_ptr<mp::SettingsHandler> handler = nullptr;
    grab_registered_persistent_handler(handler);
    mp::client::register_settings_handlers();

    EXPECT_CALL(*mock_qsettings, setValue(Eq(key), Eq(native_val)));
    inject_mock_qsettings();

    ASSERT_NO_THROW(handler->set(key, val));
}

TEST_F(TestRegisteredSettingsHandlers, clientsRegisterHandlerThatAcceptsBoolAutostart)
{
    std::unique_ptr<mp::SettingsHandler> handler = nullptr; // TODO@ricab try to extract this stuff
    grab_registered_persistent_handler(handler);
    mp::client::register_settings_handlers();

    EXPECT_CALL(*mock_qsettings, setValue(Eq(mp::autostart_key), Eq("false")));
    inject_mock_qsettings();

    ASSERT_NO_THROW(handler->set(mp::autostart_key, "0"));
}

struct TestGoodPetEnvSetting : public TestRegisteredSettingsHandlers, WithParamInterface<const char*>
{
};

TEST_P(TestGoodPetEnvSetting, clientsRegisterHandlerThatAcceptsValidPetenv)
{
    auto key = mp::petenv_key, val = GetParam();
    std::unique_ptr<mp::SettingsHandler> handler = nullptr; // TODO@ricab try to extract this stuff
    grab_registered_persistent_handler(handler);
    mp::client::register_settings_handlers();

    EXPECT_CALL(*mock_qsettings, setValue(Eq(key), Eq(val)));
    inject_mock_qsettings();

    ASSERT_NO_THROW(handler->set(key, val));
}

INSTANTIATE_TEST_SUITE_P(TestGoodPetEnvSetting, TestGoodPetEnvSetting, Values("valid-primary", ""));

struct TestBadPetEnvSetting : public TestRegisteredSettingsHandlers, WithParamInterface<const char*>
{
};

TEST_P(TestBadPetEnvSetting, clientsRegisterHandlerThatRejectsInvalidPetenv)
{
    auto key = mp::petenv_key, val = GetParam();
    std::unique_ptr<mp::SettingsHandler> handler = nullptr;
    grab_registered_persistent_handler(handler);
    mp::client::register_settings_handlers();

    MP_ASSERT_THROW_THAT(handler->set(key, val), mp::InvalidSettingException,
                         mpt::match_what(AllOf(HasSubstr(key), HasSubstr(val))));
}

INSTANTIATE_TEST_SUITE_P(TestBadPetEnvSetting, TestBadPetEnvSetting, Values("-", "-a-b-", "_asd", "_1", "1-2-3"));

TEST_F(TestRegisteredSettingsHandlers, daemonRegistersPersistentHandlerWithDaemonFilename)
{
    auto config_location = QStringLiteral("/a/b/c");
    auto expected_filename = config_location + "/multipassd.conf";

    EXPECT_CALL(mock_platform, daemon_config_home).WillOnce(Return(config_location));

    std::unique_ptr<mp::SettingsHandler> handler = nullptr;
    grab_registered_persistent_handler(handler);
    mp::daemon::register_settings_handlers();

    EXPECT_CALL(*mock_qsettings_provider, make_wrapped_qsettings(Eq(expected_filename), _))
        .WillOnce(InvokeWithoutArgs(make_default_returning_mock_qsettings));
    handler->set(mp::bridged_interface_key, "bridge");
}

TEST_F(TestRegisteredSettingsHandlers, daemonRegistersPersistentHandlerForDaemonSettings)
{
    const auto driver = "conductor";
    const auto mount = "false";

    EXPECT_CALL(mock_platform, default_driver).WillOnce(Return(driver));
    EXPECT_CALL(mock_platform, default_privileged_mounts).WillOnce(Return(mount));

    std::unique_ptr<mp::SettingsHandler> handler = nullptr;
    grab_registered_persistent_handler(handler);
    mp::daemon::register_settings_handlers();

    inject_default_returning_mock_qsettings();
    EXPECT_EQ(handler->get(mp::driver_key), driver);
    EXPECT_EQ(handler->get(mp::bridged_interface_key), "");
    EXPECT_EQ(handler->get(mp::mounts_key), mount);
}

TEST_F(TestRegisteredSettingsHandlers, daemonRegistersPersistentHandlerForDaemonPlatformSettings)
{
    const auto daemon_defaults = std::map<QString, QString>{{"local.blah", "blargh"},
                                                            {"local.a.bool", "false"},
                                                            {"local.foo", "barrrr"},
                                                            {"local.a.long.number", "1234567890"}};
    mp::SettingSpec::Set daemon_settings;
    for (const auto& [k, v] : daemon_defaults)
        daemon_settings.insert(std::make_unique<mp::BasicSettingSpec>(k, v));

    EXPECT_CALL(mock_platform, extra_daemon_settings).WillOnce(Return(ByMove(std::move(daemon_settings))));

    std::unique_ptr<mp::SettingsHandler> handler = nullptr;
    grab_registered_persistent_handler(handler);
    mp::daemon::register_settings_handlers();

    inject_default_returning_mock_qsettings();

    for (const auto& [k, v] : daemon_defaults)
    {
        EXPECT_EQ(handler->get(k), v);
    }
}

TEST_F(TestRegisteredSettingsHandlers, daemonRegistersPersistentHandlerWithOverriddenPlatformDefaults)
{
    // TODO@ricab
}

TEST_F(TestRegisteredSettingsHandlers, daemonDoesNotRegisterPersistentHandlerForClientSettings)
{
    // TODO@ricab
}

TEST_F(TestRegisteredSettingsHandlers, daemonRegistersHandlerThatAcceptsValidBackend)
{
    auto key = mp::driver_key, val = "good driver";

    std::unique_ptr<mp::SettingsHandler> handler = nullptr;
    grab_registered_persistent_handler(handler);
    mp::daemon::register_settings_handlers();

    EXPECT_CALL(mock_platform, is_backend_supported(Eq(val))).WillOnce(Return(true));
    EXPECT_CALL(*mock_qsettings, setValue(Eq(key), Eq(val)));
    inject_mock_qsettings();

    ASSERT_NO_THROW(handler->set(key, val));
}

TEST_F(TestRegisteredSettingsHandlers, daemonRegistersHandlerThatRejectsInvalidBackend)
{
    auto key = mp::driver_key, val = "bad driver";

    std::unique_ptr<mp::SettingsHandler> handler = nullptr;
    grab_registered_persistent_handler(handler);
    mp::daemon::register_settings_handlers();

    EXPECT_CALL(mock_platform, is_backend_supported(Eq(val))).WillOnce(Return(false));

    MP_ASSERT_THROW_THAT(handler->set(key, val), mp::InvalidSettingException,
                         mpt::match_what(AllOf(HasSubstr(key), HasSubstr(val))));
}

} // namespace
