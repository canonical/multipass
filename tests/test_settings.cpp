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
#include "mock_file_ops.h"
#include "mock_platform.h"
#include "mock_settings.h"
#include "mock_singleton_helpers.h"

#include <src/utils/wrapped_qsettings.h>

#include <multipass/constants.h>
#include <multipass/platform.h>

#include <QKeySequence>
#include <QString>

#include <fstream>

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
    MOCK_METHOD0(sync, void());
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

    void inject_real_settings_get(const QString& key)
    {
        EXPECT_CALL(mock_settings, get(Eq(key))).WillOnce(call_real_settings_get);
    }

    void inject_real_settings_set(const QString& key, const QString& val)
    {
        EXPECT_CALL(mock_settings, set(Eq(key), Eq(val))).WillOnce(call_real_settings_set);
    }

    void mock_unreadable_settings_file(const char* filename)
    {
        std::fstream fstream{};
        fstream.setstate(std::ios_base::failbit);

        EXPECT_CALL(*mock_file_ops, open(_, StrEq(filename), Eq(std::ios_base::in)))
            .WillOnce(DoAll(WithArg<0>([](auto& stream) { stream.setstate(std::ios_base::failbit); }),
                            Assign(&errno, EACCES)));

        EXPECT_CALL(*mock_qsettings, fileName).WillOnce(Return(filename));
    }

public:
    mpt::MockFileOps::GuardedMock mock_file_ops_injection = mpt::MockFileOps::inject<NiceMock>();
    mpt::MockFileOps* mock_file_ops = mock_file_ops_injection.first;

    MockQSettingsProvider::GuardedMock mock_qsettings_injection = MockQSettingsProvider::inject<StrictMock>(); /* this
    is made strict to ensure that, other than explicitly injected, no QSettings are used; that's particularly important
    when injecting real get and set behavior (don't want tests to be affected, nor themselves affect, disk state) */
    MockQSettingsProvider* mock_qsettings_provider = mock_qsettings_injection.first;

    std::unique_ptr<NiceMock<MockQSettings>> mock_qsettings = std::make_unique<NiceMock<MockQSettings>>();
    mpt::MockSettings& mock_settings = mpt::MockSettings::mock_instance();

private:
    static QString call_real_settings_get(const QString& key)
    {
        return MP_SETTINGS.Settings::get(key); // invoke the base
    }

    static void call_real_settings_set(const QString& key, const QString& val)
    {
        MP_SETTINGS.Settings::set(key, val); // invoke the base
    }
};

TEST_F(TestSettings, getReadsUtf8)
{
    const auto key = mp::petenv_key;
    EXPECT_CALL(*mock_qsettings, setIniCodec(StrEq("UTF-8"))).Times(1);

    inject_mock_qsettings();
    inject_real_settings_get(key);

    MP_SETTINGS.get(key);
}

TEST_F(TestSettings, setWritesUtf8)
{
    const auto key = mp::bridged_interface_key, val = "foo";
    EXPECT_CALL(*mock_qsettings, setIniCodec(StrEq("UTF-8"))).Times(1);

    inject_mock_qsettings();
    inject_real_settings_set(key, val);

    MP_SETTINGS.set(key, val);
}

TEST_F(TestSettings, getThrowsOnUnreadableFile)
{
    const auto key = mp::hotkey_key;

    mock_unreadable_settings_file("/an/unreadable/file");
    inject_mock_qsettings();
    inject_real_settings_get(key);

    MP_EXPECT_THROW_THAT(MP_SETTINGS.get(key), mp::PersistentSettingsException,
                         mpt::match_what(AllOf(HasSubstr("read"), HasSubstr("access"))));
}

TEST_F(TestSettings, setThrowsOnUnreadableFile)
{
    const auto key = mp::mounts_key, val = "yes";

    mock_unreadable_settings_file("unreadable");
    inject_mock_qsettings();
    inject_real_settings_set(key, val);

    MP_EXPECT_THROW_THAT(MP_SETTINGS.set(key, val), mp::PersistentSettingsException,
                         mpt::match_what(AllOf(HasSubstr("read"), HasSubstr("access"))));
}

using DescribedQSettingsStatus = std::tuple<QSettings::Status, std::string>;
struct TestSettingsReadWriteError : public TestSettings, public WithParamInterface<DescribedQSettingsStatus>
{
};

TEST_P(TestSettingsReadWriteError, getThrowsOnFileReadError)
{
    const auto& [status, desc] = GetParam();
    const auto key = multipass::driver_key;
    EXPECT_CALL(*mock_qsettings, status).WillOnce(Return(status));

    inject_mock_qsettings();
    inject_real_settings_get(key);

    MP_EXPECT_THROW_THAT(MP_SETTINGS.get(key), mp::PersistentSettingsException,
                         mpt::match_what(AllOf(HasSubstr("read"), HasSubstr(desc))));
}

TEST_P(TestSettingsReadWriteError, setThrowsOnFileWriteError)
{
    const auto& [status, desc] = GetParam();
    const auto key = mp::hotkey_key, val = "Esc";

    {
        InSequence seq;
        EXPECT_CALL(*mock_qsettings, sync).Times(1); // needs to flush to ensure failure to write
        EXPECT_CALL(*mock_qsettings, status).WillOnce(Return(status));
    }

    inject_mock_qsettings();
    inject_real_settings_set(key, val);

    MP_EXPECT_THROW_THAT(MP_SETTINGS.set(key, val), mp::PersistentSettingsException,
                         mpt::match_what(AllOf(HasSubstr("write"), HasSubstr(desc))));
}

INSTANTIATE_TEST_SUITE_P(TestSettingsAllReadErrors, TestSettingsReadWriteError,
                         Values(DescribedQSettingsStatus{QSettings::FormatError, "format"},
                                DescribedQSettingsStatus{QSettings::AccessError, "access"}));

struct TestSettingsGetRegularKeys : public TestSettings, public WithParamInterface<QString>
{
};

TEST_P(TestSettingsGetRegularKeys, getReturnsRecordedSetting)
{
    const auto key = GetParam();
    const auto val = "asdf";
    EXPECT_CALL(*mock_qsettings, value_impl(Eq(key), _)).WillOnce(Return(val));

    inject_mock_qsettings();
    inject_real_settings_get(key);

    ASSERT_NE(val, mpt::MockSettings::mock_instance().get_default(key));
    EXPECT_EQ(MP_SETTINGS.get(key), QString{val});
}

INSTANTIATE_TEST_SUITE_P(TestSettingsGetRegularKeys, TestSettingsGetRegularKeys, ValuesIn([] {
                             std::vector<QString> ret{mp::petenv_key,
                                                      mp::driver_key,
                                                      mp::autostart_key,
                                                      mp::hotkey_key,
                                                      mp::bridged_interface_key,
                                                      mp::mounts_key};

                             for (const auto& item : mp::platform::extra_settings_defaults())
                                 ret.push_back(item.first);

                             return ret;
                         }()));

using KeyVal = std::tuple<QString, QString>;
struct TestSettingsSetRegularKeys : public TestSettings, public WithParamInterface<KeyVal>
{
};

TEST_P(TestSettingsSetRegularKeys, setRecordsProvidedSetting)
{
    const auto& [key, val] = GetParam();
    EXPECT_CALL(*mock_qsettings, setValue(Eq(key), Eq(val))).Times(1);

    inject_mock_qsettings();
    inject_real_settings_set(key, val);

    ASSERT_NO_THROW(MP_SETTINGS.set(key, val));
}

INSTANTIATE_TEST_SUITE_P(TestSettingsSetRegularKeys, TestSettingsSetRegularKeys,
                         Values(KeyVal{mp::petenv_key, "instance-name"}, KeyVal{mp::petenv_key, ""},
                                KeyVal{mp::autostart_key, "false"}, KeyVal{mp::bridged_interface_key, "iface"},
                                KeyVal{mp::mounts_key, "true"}));

TEST_F(TestSettings, setTranslatesHotkey)
{
    const auto key = mp::hotkey_key;
    const auto val = "Alt+X";
    const auto native_val = mp::platform::interpret_setting(key, val);

    EXPECT_CALL(*mock_qsettings, setValue(Eq(key), Eq(native_val))).Times(1);

    inject_mock_qsettings();
    inject_real_settings_set(key, val);

    ASSERT_NO_THROW(MP_SETTINGS.set(key, val));
}

TEST_F(TestSettings, setAcceptsValidBackend)
{
    auto key = mp::driver_key, val = "good driver";

    auto [mock_platform, guard] = mpt::MockPlatform::inject();
    EXPECT_CALL(*mock_platform, is_backend_supported(Eq(val))).WillOnce(Return(true));

    EXPECT_CALL(*mock_qsettings, setValue(Eq(key), Eq(val))).Times(1);

    inject_mock_qsettings();
    inject_real_settings_set(key, val);

    ASSERT_NO_THROW(MP_SETTINGS.set(key, val));
}

TEST_F(TestSettings, setRejectsInvalidBackend)
{
    auto key = mp::driver_key, val = "bad driver";

    auto [mock_platform, guard] = mpt::MockPlatform::inject();
    EXPECT_CALL(*mock_platform, is_backend_supported(Eq(val))).WillOnce(Return(false));

    inject_real_settings_set(key, val);

    MP_ASSERT_THROW_THAT(MP_SETTINGS.set(key, val), mp::InvalidSettingsException,
                         mpt::match_what(AllOf(HasSubstr(key), HasSubstr(val))));
}

using KeyReprVal = std::tuple<QString, QString, QString>;
struct TestSettingsGoodBoolConversion : public TestSettings, public WithParamInterface<KeyReprVal>
{
};

TEST_P(TestSettingsGoodBoolConversion, setConvertsAcceptableBoolRepresentations)
{
    const auto& [key, repr, val] = GetParam();
    EXPECT_CALL(*mock_qsettings, setValue(Eq(key), Eq(val))).Times(1);

    inject_mock_qsettings();
    inject_real_settings_set(key, repr);

    ASSERT_NO_THROW(MP_SETTINGS.set(key, repr));
}

INSTANTIATE_TEST_SUITE_P(TestSettingsGoodBool, TestSettingsGoodBoolConversion, ValuesIn([] {
                             std::vector<KeyReprVal> ret;
                             for (const auto& key : {mp::autostart_key, mp::mounts_key})
                             {
                                 for (const auto& repr : {"yes", "on", "1", "true"})
                                     ret.emplace_back(key, repr, "true");
                                 for (const auto& repr : {"no", "off", "0", "false"})
                                     ret.emplace_back(key, repr, "false");
                             }

                             return ret;
                         }()));

struct TestSettingsBadValue : public TestSettings, WithParamInterface<KeyVal>
{
};

TEST_P(TestSettingsBadValue, setThrowsOnInvalidSettingValue)
{
    const auto& [key, val] = GetParam();

    inject_real_settings_set(key, val);

    MP_ASSERT_THROW_THAT(MP_SETTINGS.set(key, val), mp::InvalidSettingsException,
                         mpt::match_what(AllOf(HasSubstr(key.toStdString()), HasSubstr(val.toStdString()))));
}

INSTANTIATE_TEST_SUITE_P(TestSettingsBadBool, TestSettingsBadValue,
                         Combine(Values(mp::autostart_key, mp::mounts_key),
                                 Values("nonsense", "invalid", "", "bool", "representations", "-", "null")));

INSTANTIATE_TEST_SUITE_P(TestSettingsBadPetEnv, TestSettingsBadValue,
                         Combine(Values(mp::petenv_key), Values("-", "-a-b-", "_asd", "_1", "1-2-3")));

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
    return {{false, {"False", "false", "0", ""}}, {true, {"True", "true", "1", "no", "off", "anything else"}}};
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
struct TestSuccessfulSettingsGetAs : public TestSettings
{
};

using GetAsTestTypes = ::testing::Types<bool, int, QKeySequence>; // to add more, specialize setting_val_reprs above
MP_TYPED_TEST_SUITE(TestSuccessfulSettingsGetAs, GetAsTestTypes);

TYPED_TEST(TestSuccessfulSettingsGetAs, getAsConvertsValues)
{
    InSequence seq;
    const auto key = "whatever";
    for (const auto& [val, reprs] : setting_val_reprs<TypeParam>())
    {
        for (const auto& repr : reprs)
        {
            EXPECT_CALL(mpt::MockSettings::mock_instance(), get(Eq(key))).WillOnce(Return(repr));
            EXPECT_EQ(MP_SETTINGS.get_as<TypeParam>(key), val);
        }
    }
}

TEST_F(TestSettings, getAsThrowsOnUnsupportedTypeConversion)
{
    const auto key = "the.key";
    const auto bad_repr = "#$%!@";
    EXPECT_CALL(mpt::MockSettings::mock_instance(), get(Eq(key))).WillOnce(Return(bad_repr));
    MP_ASSERT_THROW_THAT(MP_SETTINGS.get_as<QVariant>(key), mp::UnsupportedSettingValueType<QVariant>,
                         mpt::match_what(HasSubstr(key)));
}

TEST_F(TestSettings, getAsReturnsDefaultOnBadValue)
{
    const auto key = "a.key";
    const auto bad_int = "ceci n'est pas une int";
    EXPECT_CALL(mpt::MockSettings::mock_instance(), get(Eq(key))).WillOnce(Return(bad_int));
    EXPECT_EQ(MP_SETTINGS.get_as<int>(key), 0);
}

// Tests for mock settings
TEST(MockSettings, providesGetDefaultAsGetByDefault)
{
    const auto& key = mp::driver_key;
    ASSERT_EQ(MP_SETTINGS.get(key), mpt::MockSettings::mock_instance().get_default(key));
}

TEST(MockSettings, canHaveGetMocked)
{
    const auto test = QStringLiteral("abc"), proof = QStringLiteral("xyz");
    const auto& mock = mpt::MockSettings::mock_instance();

    EXPECT_CALL(mock, get(test)).WillOnce(Return(proof));
    ASSERT_EQ(MP_SETTINGS.get(test), proof);
}

} // namespace
