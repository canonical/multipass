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
#include "mock_singleton_helpers.h"

#include <src/utils/wrapped_qsettings.h>

#include <multipass/constants.h>
#include <multipass/exceptions/settings_exceptions.h>
#include <multipass/optional.h>
#include <multipass/persistent_settings_handler.h>

#include <QString>

#include <cstdio>

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

class TestPersistentSettingsHandler : public Test
{
public:
    mp::PersistentSettingsHandler make_handler(const mp::optional<QString>& specific_key = mp::nullopt,
                                               const mp::optional<QString>& specific_val = mp::nullopt)
    {
        if (specific_key)
            defaults[*specific_key] = specific_val.value_or("banana");

        return mp::PersistentSettingsHandler{fake_filename, defaults};
    }

    void inject_mock_qsettings() // moves the mock, so call once only, after setting expectations
    {
        EXPECT_CALL(*mock_qsettings_provider, make_wrapped_qsettings(_, Eq(QSettings::IniFormat)))
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

    MockQSettingsProvider::GuardedMock mock_qsettings_injection = MockQSettingsProvider::inject<StrictMock>(); /* strict
                                                to ensure that, other than explicitly injected, no QSettings are used */
    MockQSettingsProvider* mock_qsettings_provider = mock_qsettings_injection.first;

    std::unique_ptr<NiceMock<MockQSettings>> mock_qsettings = std::make_unique<NiceMock<MockQSettings>>();
};

TEST_F(TestPersistentSettingsHandler, getReadsUtf8)
{
    const auto key = "asdf";
    EXPECT_CALL(*mock_qsettings, setIniCodec(StrEq("UTF-8"))).Times(1);

    inject_mock_qsettings();

    make_handler(key).get(key);
}

TEST_F(TestPersistentSettingsHandler, setWritesUtf8)
{
    const auto key = "a.key";
    EXPECT_CALL(*mock_qsettings, setIniCodec(StrEq("UTF-8"))).Times(1);

    inject_mock_qsettings();

    make_handler(key).set(key, "kokoko");
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
    const auto handler = make_handler(key, val);

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
    const auto handler = make_handler(key);

    {
        InSequence seq;
        EXPECT_CALL(*mock_qsettings, sync).Times(1); // needs to flush to ensure failure to write
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
    const auto handler = make_handler();

    EXPECT_CALL(*mock_qsettings_provider, make_wrapped_qsettings).Times(0);
    MP_EXPECT_THROW_THAT(handler.set(key, "asdf"), mp::UnrecognizedSettingException, mpt::match_what(HasSubstr(key)));
}

TEST_F(TestPersistentSettingsHandler, setRecordsProvidedSetting)
{
    const auto key = "name.a.key", val = "and a value";
    const auto handler = make_handler(key);
    EXPECT_CALL(*mock_qsettings, setValue(Eq(key), Eq(val))).Times(1);

    inject_mock_qsettings();

    ASSERT_NO_THROW(handler.set(key, val));
}

// TODO@ricab check filename

} // namespace
