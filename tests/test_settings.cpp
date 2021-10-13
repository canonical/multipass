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
#include "disabling_macros.h"
#include "mock_settings.h"

#include <src/utils/qsettings_wrapper.h>

#include <multipass/constants.h>
#include <multipass/platform.h>
#include <multipass/settings.h>

#include <QKeySequence>
#include <QString>

namespace mp = multipass;
namespace mpt = mp::test;
using namespace testing;

namespace
{
class MockQSettings : public mp::WrappedQSettings
{
public:
    using WrappedQSettings::WrappedQSettings; // promote visibility
    MOCK_CONST_METHOD0(status, QSettings::Status());
    MOCK_CONST_METHOD0(fileName, QString());
    MOCK_CONST_METHOD2(value_impl, QVariant(const QString& key, const QVariant& default_value)); // promote visibility
    MOCK_METHOD1(setIniCodec, void(const char* codec_name));
    MOCK_METHOD0(sync, void()); // TODO@ricab verify all these
    MOCK_METHOD2(setValue, void(const QString& key, const QVariant& value));
};

class MockQSettingsProvider : public mp::WrappedQSettingsFactory
{
public:
    using WrappedQSettingsFactory::WrappedQSettingsFactory;
    MOCK_CONST_METHOD2(make_wrapped_qsettings,
                       std::unique_ptr<mp::WrappedQSettings>(const QString&, QSettings::Format));

    MP_MOCK_SINGLETON_BOILERPLATE(MockQSettingsProvider, WrappedQSettingsFactory);
};

struct TestSettings : public Test
{
    void inject_mock_qsettings() // moves the mock, so call once only, after setting expectations
    {
        EXPECT_CALL(*mock_qsettings_provider, make_wrapped_qsettings(_, Eq(QSettings::IniFormat)))
            .WillOnce(Return(ByMove(std::move(mock_qsettings))));
    }

    static QString call_real_settings_get(const QString& key)
    {
        return MP_SETTINGS.Settings::get(key); // invoke the base
    }

    MockQSettingsProvider::GuardedMock mock_qsettings_injection = MockQSettingsProvider::inject();
    MockQSettingsProvider* mock_qsettings_provider = mock_qsettings_injection.first;
    std::unique_ptr<MockQSettings> mock_qsettings = std::make_unique<MockQSettings>(); // TODO@ricab nice?
};

TEST_F(TestSettings, getReadsUtf8)
{
    auto key = mp::petenv_key;
    EXPECT_CALL(*mock_qsettings, setIniCodec(StrEq("UTF-8"))).Times(1);

    inject_mock_qsettings();
    EXPECT_CALL(mpt::MockSettings::mock_instance(), get(Eq(key))).WillOnce(call_real_settings_get);

    MP_SETTINGS.get(key);
}

TEST_F(TestSettings, DISABLE_ON_WINDOWS(getThrowsOnUnreadableFile))
{
    auto key = multipass::hotkey_key;
    EXPECT_CALL(*mock_qsettings, fileName).WillOnce(Return("/root/asdf"));

    inject_mock_qsettings();
    EXPECT_CALL(mpt::MockSettings::mock_instance(), get(Eq(key))).WillOnce(call_real_settings_get);

    MP_EXPECT_THROW_THAT(MP_SETTINGS.get(key), mp::PersistentSettingsException,
                         mpt::match_what(AllOf(HasSubstr("read"), HasSubstr("access"))));

    EXPECT_EQ(errno, EACCES) << "errno is " << errno;
}

struct DescribedQSettingsStatus
{
    QSettings::Status status;
    std::string description;
};

struct TestSettingsReadError : public TestSettings, public WithParamInterface<DescribedQSettingsStatus>
{
};

TEST_P(TestSettingsReadError, getThrowsOnFileReadError)
{
    const auto& [status, desc] = GetParam();
    auto key = multipass::driver_key;
    EXPECT_CALL(*mock_qsettings, status).WillOnce(Return(status));

    inject_mock_qsettings();
    EXPECT_CALL(mpt::MockSettings::mock_instance(), get(Eq(key))).WillOnce(call_real_settings_get);

    MP_EXPECT_THROW_THAT(MP_SETTINGS.get(key), mp::PersistentSettingsException,
                         mpt::match_what(AllOf(HasSubstr("read"), HasSubstr(desc))));
}

INSTANTIATE_TEST_SUITE_P(TestSettingsAllReadErrors, TestSettingsReadError,
                         Values(DescribedQSettingsStatus{QSettings::FormatError, "format"},
                                DescribedQSettingsStatus{QSettings::AccessError, "access"}));

struct TestSettingsKeyParam : public TestSettings, public WithParamInterface<QString>
{
};

TEST_P(TestSettingsKeyParam, getReturnsRecordedSetting)
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

INSTANTIATE_TEST_SUITE_P(TestSettingsRegularKeys, TestSettingsKeyParam, ValuesIn(get_regular_keys()));

template <typename T>
struct SettingValueRepresentation
{
    T val;
    QStringList reprs;
};

template <typename T>
std::vector<SettingValueRepresentation<T>> setting_val_reprs();

template <>
std::vector<SettingValueRepresentation<bool>> setting_val_reprs()
{
    return {{false, {"False", "false", "0"}}, {true, {"True", "true", "1"}}};
}

template <>
std::vector<SettingValueRepresentation<int>> setting_val_reprs()
{
    return {{0, {"0", "+0", "-0000"}}, {42, {"42", "+42"}}, {-2, {"-2"}}, {23, {"023"}}}; // no hex or octal
}

template <>
std::vector<SettingValueRepresentation<QKeySequence>> setting_val_reprs()
{
    return {{QKeySequence{"Ctrl+Alt+O", QKeySequence::NativeText}, {"Ctrl+Alt+O", "ctrl+alt+o"}},
            {QKeySequence{"shift+e", QKeySequence::NativeText}, {"Shift+E", "shift+e"}}}; /*
                                  only a couple here, don't wanna go into platform details */
}

template <typename T>
struct TestSettingsGetAs : public TestSettings
{
};

using GetAsTestTypes = ::testing::Types<bool, int, QKeySequence>; // to add more, specialize setting_val_reprs above
MP_TYPED_TEST_SUITE(TestSettingsGetAs, GetAsTestTypes);

TYPED_TEST(TestSettingsGetAs, getAsConvertsValues)
{
    InSequence seq;
    auto key = "whatever";
    for (const auto& [val, reprs] : setting_val_reprs<TypeParam>())
    {
        for (const auto& repr : reprs)
        {
            EXPECT_CALL(mpt::MockSettings::mock_instance(), get(Eq(key))).WillOnce(Return(repr));
            EXPECT_EQ(MP_SETTINGS.get_as<TypeParam>(key), val);
        }
    }
}

TEST(MockSettings, providesGetDefaultAsGetByDefault)
{
    const auto& key = mp::driver_key;
    ASSERT_EQ(MP_SETTINGS.get(key), mpt::MockSettings::mock_instance().get_default(key));
}

TEST(MockSettings, canHaveGetMocked)
{
    const auto test = QStringLiteral("abc"), proof = QStringLiteral("xyz");
    auto& mock = mpt::MockSettings::mock_instance();

    EXPECT_CALL(mock, get(test)).WillOnce(Return(proof));
    ASSERT_EQ(MP_SETTINGS.get(test), proof);
}

} // namespace
