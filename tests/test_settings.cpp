/*
 * Copyright (C) 2019-2021 Canonical, Ltd.
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
#include "mock_settings.h"

#include <src/utils/qsettings_wrapper.h>

#include <multipass/constants.h>
#include <multipass/platform.h>
#include <multipass/settings.h>

#include <QString>

namespace mp = multipass;
namespace mpt = mp::test;
using namespace testing;

namespace
{
class MockQSettingsWrapper : public mp::QSettingsWrapper
{
public:
    using QSettingsWrapper::QSettingsWrapper; // promote visibility
    MOCK_CONST_METHOD0(status, QSettings::Status());
    MOCK_CONST_METHOD0(fileName, QString());
    MOCK_CONST_METHOD2(value_impl, QVariant(const QString& key, const QVariant& default_value)); // promote visibility
    MOCK_METHOD1(setIniCodec, void(const char* codec_name));
    MOCK_METHOD0(sync, void());
    MOCK_METHOD2(setValue, void(const QString& key, const QVariant& value));
};

class MockQSettingsProvider : public mp::QSettingsProvider
{
public:
    using QSettingsProvider::QSettingsProvider;
    MOCK_CONST_METHOD2(make_qsettings_wrapper,
                       std::unique_ptr<mp::QSettingsWrapper>(const QString&, QSettings::Format));

    MP_MOCK_SINGLETON_BOILERPLATE(MockQSettingsProvider, QSettingsProvider);
};

struct SettingsTest : public TestWithParam<QString>
{
    void inject_mock_qsettings() // moves the mock, so call once only, after setting expectations
    {
        EXPECT_CALL(*mock_qsettings_provider, make_qsettings_wrapper(_, Eq(QSettings::IniFormat)))
            .WillOnce(Return(ByMove(std::move(mock_qsettings))));
    }

    static QString call_real_settings_get(const QString& key)
    {
        return MP_SETTINGS.Settings::get(key); // invoke the base
    }

    MockQSettingsProvider::GuardedMock mock_qsettings_injection = MockQSettingsProvider::inject();
    MockQSettingsProvider* mock_qsettings_provider = mock_qsettings_injection.first;
    std::unique_ptr<MockQSettingsWrapper> mock_qsettings = std::make_unique<MockQSettingsWrapper>();
};

TEST_P(SettingsTest, get_reads_utf8)
{
    auto key = GetParam();
    EXPECT_CALL(*mock_qsettings, setIniCodec(StrEq("UTF-8"))).Times(1);

    inject_mock_qsettings();
    EXPECT_CALL(mpt::MockSettings::mock_instance(), get(Eq(key))).WillOnce(call_real_settings_get);

    MP_SETTINGS.get(key);
}

TEST_P(SettingsTest, get_returns_recorded_setting)
{
    auto key = GetParam();
    auto val = "asdf";
    EXPECT_CALL(*mock_qsettings, value_impl(Eq(key), _)).WillOnce(Return(val));

    inject_mock_qsettings();
    EXPECT_CALL(mpt::MockSettings::mock_instance(), get(Eq(key))).WillOnce(call_real_settings_get);

    ASSERT_NE(val, mpt::MockSettings::mock_instance().get_default(key));
    EXPECT_EQ(MP_SETTINGS.get(key), QString{val});
}

std::vector<QString> get_regular_keys()
{
    std::vector<QString> ret{
        mp::petenv_key, mp::driver_key, mp::autostart_key, mp::hotkey_key, mp::bridged_interface_key, mp::mounts_key};

    for (const auto& item : mp::platform::extra_settings_defaults())
        ret.push_back(item.first);

    return ret;
}

INSTANTIATE_TEST_SUITE_P(SettingsTestAllKeys, SettingsTest, ValuesIn(get_regular_keys()));

TEST(MockSettings, provides_get_default_as_get_by_default)
{
    const auto& key = mp::driver_key;
    ASSERT_EQ(MP_SETTINGS.get(key), mpt::MockSettings::mock_instance().get_default(key));
}

TEST(MockSettings, can_have_get_mocked)
{
    const auto test = QStringLiteral("abc"), proof = QStringLiteral("xyz");
    auto& mock = mpt::MockSettings::mock_instance();

    EXPECT_CALL(mock, get(test)).WillOnce(Return(proof));
    ASSERT_EQ(MP_SETTINGS.get(test), proof);
}

} // namespace
