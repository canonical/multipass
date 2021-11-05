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
#include "mock_singleton_helpers.h"

#include <src/utils/wrapped_qsettings.h>

#include <multipass/constants.h>
#include <multipass/exceptions/settings_exceptions.h>
#include <multipass/persistent_settings_handler.h>

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
    void inject_mock_qsettings() // moves the mock, so call once only, after setting expectations
    {
        EXPECT_CALL(*mock_qsettings_provider, make_wrapped_qsettings(_, Eq(QSettings::IniFormat)))
            .WillOnce(Return(ByMove(std::move(mock_qsettings))));
    }

public:
    MockQSettingsProvider::GuardedMock mock_qsettings_injection = MockQSettingsProvider::inject<StrictMock>(); /* strict
                                                to ensure that, other than explicitly injected, no QSettings are used */
    MockQSettingsProvider* mock_qsettings_provider = mock_qsettings_injection.first;
    std::unique_ptr<NiceMock<MockQSettings>> mock_qsettings = std::make_unique<NiceMock<MockQSettings>>();
};

TEST_F(TestPersistentSettingsHandler, getReadsUtf8)
{
    const auto key = "asdf";
    mp::PersistentSettingsHandler handler{"", {{key, ""}}};
    EXPECT_CALL(*mock_qsettings, setIniCodec(StrEq("UTF-8"))).Times(1);

    inject_mock_qsettings();

    handler.get("asdf");
}
} // namespace
