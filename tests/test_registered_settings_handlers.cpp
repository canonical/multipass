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
#include <multipass/persistent_settings_handler.h>

#include <src/daemon/daemon_init_settings.h>

#include <QKeySequence>

namespace mp = multipass;
namespace mpt = mp::test;
using namespace testing;

namespace
{
struct TestRegisteredSettingsHandlers : public Test
{
    void inject_default_returning_mock_qsettings() // moves the mock, so call once only, after setting expectations
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

public:
    mpt::MockQSettingsProvider::GuardedMock mock_qsettings_injection =
        mpt::MockQSettingsProvider::inject<StrictMock>(); /* strict to ensure that, other than explicitly injected, no
                                                             QSettings are used */
    mpt::MockQSettingsProvider* mock_qsettings_provider = mock_qsettings_injection.first;

    mpt::MockSettings& mock_settings = mpt::MockSettings::mock_instance();
};

TEST_F(TestRegisteredSettingsHandlers, clients_register_persistent_handler_with_client_filename)
{
    auto config_location = QStringLiteral("/a/b/c");
    auto expected_filename = config_location + "/multipass/multipass.conf";
    EXPECT_CALL(mpt::MockStandardPaths::mock_instance(), writableLocation(mp::StandardPaths::GenericConfigLocation))
        .WillOnce(Return(config_location));

    std::unique_ptr<mp::SettingsHandler> handler = nullptr;
    EXPECT_CALL(mock_settings, register_handler(NotNull())) // TODO@ricab will need to distinguish types (need #2282)
        .WillOnce([&handler](auto&& arg) { handler = std::move(arg); })
        .WillOnce(Return()) // TODO@ricab drop this when daemon handler is gone
        ;

    mp::client::register_settings_handlers();
    ASSERT_THAT(dynamic_cast<mp::PersistentSettingsHandler*>(handler.get()), NotNull()); // TODO@ricab move into expect

    EXPECT_CALL(*mock_qsettings_provider, make_wrapped_qsettings(Eq(expected_filename), _))
        .WillOnce(InvokeWithoutArgs(make_default_returning_mock_qsettings));
    handler->set(mp::petenv_key, "goo");
}

TEST_F(TestRegisteredSettingsHandlers, clients_register_persistent_handler_for_client_settings)
{
    std::unique_ptr<mp::SettingsHandler> handler = nullptr;
    EXPECT_CALL(mock_settings, register_handler(NotNull())) // TODO@ricab will need to distinguish types (need #2282)
        .WillOnce([&handler](auto&& arg) { handler = std::move(arg); })
        .WillOnce(Return()) // TODO@ricab drop this when daemon handler is gone
        ;

    mp::client::register_settings_handlers();
    ASSERT_THAT(dynamic_cast<mp::PersistentSettingsHandler*>(handler.get()), NotNull()); // TODO@ricab move into expect

    inject_default_returning_mock_qsettings();
    EXPECT_EQ(handler->get(mp::petenv_key), "primary");
    EXPECT_EQ(handler->get(mp::autostart_key), "true");
    EXPECT_EQ(QKeySequence{handler->get(mp::hotkey_key)}, QKeySequence{mp::hotkey_default});
}

TEST_F(TestRegisteredSettingsHandlers, clients_register_persistent_handler_for_client_platform_settings)
{
    const auto client_defaults = std::map<QString, QString>{{"client.a.setting", "a reasonably long value for this"},
                                                            {"client.empty.setting", ""},
                                                            {"client.an.int", "-12345"},
                                                            {"client.a.float.with.a.long_key", "3.14"}};
    const auto other_defaults = std::map<QString, QString>{{"abc", "true"}, {"asdf", "fdsa"}};
    auto all_defaults = client_defaults;
    all_defaults.insert(other_defaults.cbegin(), other_defaults.cend());

    auto [mock_platform, guard] = mpt::MockPlatform::inject();
    EXPECT_CALL(*mock_platform, extra_settings_defaults)
        .WillOnce(Return(all_defaults))
        .WillOnce(Return(std::map<QString, QString>{})); // TODO@ricab drop this when daemon handler is gone

    // TODO@ricab refactor this stuff once following TODOs are settled (need newer gmock)
    std::unique_ptr<mp::SettingsHandler> handler = nullptr;
    EXPECT_CALL(mock_settings, register_handler(NotNull())) // TODO@ricab will need to distinguish types (need #2282)
        .WillOnce([&handler](auto&& arg) { handler = std::move(arg); })
        .WillOnce(Return()) // TODO@ricab drop this when daemon handler is gone
        ;

    mp::client::register_settings_handlers();
    ASSERT_THAT(dynamic_cast<mp::PersistentSettingsHandler*>(handler.get()), NotNull()); // TODO@ricab move into expect

    inject_default_returning_mock_qsettings();

    for (const auto& item : other_defaults)
    {
        MP_EXPECT_THROW_THAT(handler->get(item.first), mp::UnrecognizedSettingException,
                             mpt::match_what(HasSubstr(item.first.toStdString())));
    }

    for (const auto& [k, v] : client_defaults)
    {
        EXPECT_EQ(handler->get(k), v);
    }
}

TEST_F(TestRegisteredSettingsHandlers, clients_do_not_register_persistent_handler_for_daemon_settings)
{
    // TODO@ricab
}

TEST_F(TestRegisteredSettingsHandlers, daemon_registers_persistent_handler_with_daemon_filename)
{
    // TODO@ricab
}

TEST_F(TestRegisteredSettingsHandlers, daemon_registers_persistent_handler_for_daemon_settings)
{
    // TODO@ricab
}

TEST_F(TestRegisteredSettingsHandlers, daemon_registers_persistent_handler_for_daemon_platform_settings)
{
    // TODO@ricab
}

TEST_F(TestRegisteredSettingsHandlers, daemon_does_not_register_persistent_handler_for_client_settings)
{
    // TODO@ricab
}

} // namespace
