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
#include "mock_file_ops.h"
#include "mock_qsettings.h"

#include <multipass/constants.h>
#include <multipass/exceptions/settings_exceptions.h>
#include <multipass/settings/basic_setting_spec.h>
#include <multipass/settings/custom_setting_spec.h>
#include <multipass/settings/persistent_settings_handler.h>

#include <QString>

#include <functional>

namespace mp = multipass;
namespace mpt = mp::test;
using namespace testing;

namespace
{

class TestPersistentSettingsHandler : public Test
{
public:
    mp::PersistentSettingsHandler
    make_handler(const std::optional<QString>& specific_key = std::nullopt,
                 const std::optional<QString>& specific_val = std::nullopt,
                 const std::optional<std::function<QString(QString)>>& specific_interpreter = std::nullopt)
    {
        auto setting_set = make_basic_persistent_settings();

        if (specific_key)
        {
            auto val = specific_val.value_or("banana");

            if (specific_interpreter)
                setting_set.insert(std::make_unique<mp::CustomSettingSpec>(*specific_key, val, *specific_interpreter));
            else
                setting_set.insert(std::make_unique<mp::BasicSettingSpec>(*specific_key, val));
        }

        return mp::PersistentSettingsHandler{fake_filename, std::move(setting_set)};
    }

    void inject_mock_qsettings() // moves the mock, so call once only, after setting expectations
    {
        EXPECT_CALL(*mock_qsettings_provider, make_wrapped_qsettings(Eq(fake_filename), Eq(QSettings::IniFormat)))
            .WillOnce(Return(ByMove(std::move(mock_qsettings))));
    }

    void mock_unreadable_settings_file()
    {
        std::fstream fstream{};
        fstream.setstate(std::ios_base::failbit);

        EXPECT_CALL(*mock_file_ops, open(_, StrEq(qPrintable(fake_filename)), Eq(std::ios_base::in)))
            .WillOnce(DoAll(WithArg<0>([](auto& stream) { stream.setstate(std::ios_base::failbit); }),
                            Assign(&errno, EACCES)));

        EXPECT_CALL(*mock_qsettings, fileName).WillOnce(Return(fake_filename));
    }

public:
    QString fake_filename = "/tmp/fake.filename";
    std::map<QString, QString> defaults{
        {"a.key", "a value"}, {"another.key", "with a value"}, {"one.further.key", "and its default value"}};

    mpt::MockFileOps::GuardedMock mock_file_ops_injection = mpt::MockFileOps::inject<NiceMock>();
    mpt::MockFileOps* mock_file_ops = mock_file_ops_injection.first;

    mpt::MockQSettingsProvider::GuardedMock mock_qsettings_injection =
        mpt::MockQSettingsProvider::inject<StrictMock>(); /* strict to ensure that, other than explicitly injected, no
                                                             QSettings are used */
    mpt::MockQSettingsProvider* mock_qsettings_provider = mock_qsettings_injection.first;

    std::unique_ptr<NiceMock<mpt::MockQSettings>> mock_qsettings = std::make_unique<NiceMock<mpt::MockQSettings>>();

private:
    mp::SettingSpec::Set make_basic_persistent_settings() const
    {
        mp::SettingSpec::Set ret;
        for (const auto& [k, v] : defaults)
            ret.insert(std::make_unique<mp::BasicSettingSpec>(k, v));

        return ret;
    }
};

TEST_F(TestPersistentSettingsHandler, keysReturnsEmptyWhenNoSettingsSpec)
{
    mp::PersistentSettingsHandler handler{fake_filename, {}};

    EXPECT_THAT(handler.keys(), IsEmpty());
}

TEST_F(TestPersistentSettingsHandler, keysReturnsSpecsKey)
{
    auto handler = make_handler();

    std::vector<QString> expected;
    expected.reserve(defaults.size());
    for (const auto& item : defaults)
        expected.push_back(item.first);

    EXPECT_THAT(handler.keys(), UnorderedPointwise(Eq(), expected));
}

TEST_F(TestPersistentSettingsHandler, getThrowsOnUnreadableFile)
{
    const auto key = "foo";
    const auto handler = make_handler(key);

    mock_unreadable_settings_file();
    inject_mock_qsettings();

    MP_EXPECT_THROW_THAT(handler.get(key), mp::PersistentSettingsException,
                         mpt::match_what(AllOf(HasSubstr("read"), HasSubstr("access"))));
}

TEST_F(TestPersistentSettingsHandler, setThrowsOnUnreadableFile)
{
    const auto key = mp::mounts_key, val = "yes";
    auto handler = make_handler(key, val);

    mock_unreadable_settings_file();
    inject_mock_qsettings();

    MP_EXPECT_THROW_THAT(handler.set(key, val), mp::PersistentSettingsException,
                         mpt::match_what(AllOf(HasSubstr("read"), HasSubstr("access"))));
}

using DescribedQSettingsStatus = std::tuple<QSettings::Status, std::string>;
struct TestPersistentSettingsReadWriteError : public TestPersistentSettingsHandler,
                                              public WithParamInterface<DescribedQSettingsStatus>
{
};

TEST_P(TestPersistentSettingsReadWriteError, getThrowsOnFileReadError)
{
    const auto& [status, desc] = GetParam();
    const auto key = "token";
    const auto handler = make_handler(key);

    EXPECT_CALL(*mock_qsettings, status).WillOnce(Return(status));

    inject_mock_qsettings();

    MP_EXPECT_THROW_THAT(handler.get(key), mp::PersistentSettingsException,
                         mpt::match_what(AllOf(HasSubstr("read"), HasSubstr(desc))));
}

TEST_P(TestPersistentSettingsReadWriteError, setThrowsOnFileWriteError)
{
    const auto& [status, desc] = GetParam();
    const auto key = "blah";
    auto handler = make_handler(key);

    {
        InSequence seq;
        EXPECT_CALL(*mock_qsettings, sync); // needs to flush to ensure failure to write
        EXPECT_CALL(*mock_qsettings, status).WillOnce(Return(status));
    }

    inject_mock_qsettings();

    MP_EXPECT_THROW_THAT(handler.set(key, "bleh"), mp::PersistentSettingsException,
                         mpt::match_what(AllOf(HasSubstr("write"), HasSubstr(desc))));
}

INSTANTIATE_TEST_SUITE_P(TestSettingsAllReadErrors, TestPersistentSettingsReadWriteError,
                         Values(DescribedQSettingsStatus{QSettings::FormatError, "format"},
                                DescribedQSettingsStatus{QSettings::AccessError, "access"}));

TEST_F(TestPersistentSettingsHandler, getReturnsRecordedSetting)
{
    const auto key = "choose.a.key", val = "asdf", default_ = "some default";
    const auto handler = make_handler(key, default_);

    EXPECT_CALL(*mock_qsettings, value_impl(Eq(key), _)).WillOnce(Return(val));

    inject_mock_qsettings();

    ASSERT_NE(val, default_);
    EXPECT_EQ(handler.get(key), QString{val});
}

TEST_F(TestPersistentSettingsHandler, getResetsInvalidValueAndReturnsDefault)
{
    const auto key = "tampered.setting", val = "xyz", default_ = "abc", error = "nonsense";
    const auto handler = make_handler(key, default_, [&key, &val, &error](QString v) -> QString {
        if (v == val)
            throw mp::InvalidSettingException{key, val, error};

        return v;
    });

    {
        InSequence seq;
        EXPECT_CALL(*mock_qsettings, value_impl(Eq(key), _)).WillOnce(Return(val));
        EXPECT_CALL(*mock_qsettings, remove(Eq(key)));
    }

    inject_mock_qsettings();

    ASSERT_NE(val, default_);
    EXPECT_EQ(handler.get(key), QString{default_});
}

TEST_F(TestPersistentSettingsHandler, getReturnsDefaultByDefault)
{
    const auto key = "chave", default_ = "Cylinder";
    const auto handler = make_handler(key, default_);

    EXPECT_CALL(*mock_qsettings, value_impl(Eq(key), Eq(default_))).WillOnce(Return(default_));

    inject_mock_qsettings();

    ASSERT_EQ(handler.get(key), QString(default_));
}

TEST_F(TestPersistentSettingsHandler, getThrowsOnUnknownKey)
{
    const auto key = "clef";
    const auto handler = make_handler();

    EXPECT_CALL(*mock_qsettings_provider, make_wrapped_qsettings).Times(0);
    MP_EXPECT_THROW_THAT(handler.get(key), mp::UnrecognizedSettingException, mpt::match_what(HasSubstr(key)));
}

TEST_F(TestPersistentSettingsHandler, setThrowsOnUnknownKey)
{
    const auto key = "ki";
    auto handler = make_handler();

    EXPECT_CALL(*mock_qsettings_provider, make_wrapped_qsettings).Times(0);
    MP_EXPECT_THROW_THAT(handler.set(key, "asdf"), mp::UnrecognizedSettingException, mpt::match_what(HasSubstr(key)));
}

TEST_F(TestPersistentSettingsHandler, setRecordsProvidedBasicSetting)
{
    const auto key = "name.a.key", val = "and a value";
    auto handler = make_handler(key);
    EXPECT_CALL(*mock_qsettings, setValue(Eq(key), Eq(val)));

    inject_mock_qsettings();

    ASSERT_NO_THROW(handler.set(key, val));
}

TEST_F(TestPersistentSettingsHandler, setRecordsInterpretedSetting)
{
    const auto key = "k.e.y", given_val = "given", interpreted_val = "interpreted";
    auto handler = make_handler(key, "default", [&interpreted_val](QString) { return interpreted_val; });
    EXPECT_CALL(*mock_qsettings, setValue(Eq(key), Eq(interpreted_val)));

    inject_mock_qsettings();

    ASSERT_NO_THROW(handler.set(key, given_val));
}

TEST_F(TestPersistentSettingsHandler, setThrowsInterpreterExceptions)
{
    const auto key = "clave", default_ = "valid", val = "invalid", error = "nope";
    auto handler = make_handler(key, default_, [&key, &default_, &val, &error](QString v) {
        if (v == default_)
            return v;

        throw mp::InvalidSettingException{key, val, error};
    });

    EXPECT_CALL(*mock_qsettings_provider, make_wrapped_qsettings).Times(0);
    MP_EXPECT_THROW_THAT(handler.set(key, val), mp::InvalidSettingException, mpt::match_what(HasSubstr(error)));
}

} // namespace
